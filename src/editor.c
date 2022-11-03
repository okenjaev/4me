#include "editor.h"
#include "sys.h"
#include "render.h"
#include "input.h"
#include "str.h"

extern struct config config;

void
editor_set_cursor_position(const struct buffer* buffer, struct str* renderb)
{
    int y = buffer->cp.y - buffer->cp.rowoff + 1;
    int x = buffer->cp.r - buffer->cp.coloff + 1;
    char cur_pos[32];
    snprintf(cur_pos, sizeof(cur_pos), "\x1b[%d;%dH", y, x);
    str_append_raw(renderb, cur_pos, strlen(cur_pos));
}

void
editor_draw_status_bar(const struct buffer* buffer, struct str* renderb)
{
    str_append_raw(renderb, "\x1b[7m", 4);

    char status[80], rstatus[80];
    int len = snprintf(status, sizeof(status), "%.20s %s",
		       buffer->file_name ? buffer->file_name : "[empty]", buffer->dirty ? "(modified)" : "");
    int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d", buffer->cp.y + 1, buffer->num_rows);
    
    if (len > config.screencols)
    {
	len = config.screencols;
    }

    str_append_raw(renderb, status, len);
    while(len < config.screencols)
    {
	if (config.screencols - len == rlen)
	{
	    str_append_raw(renderb, rstatus, rlen);
	    break;
	}
	else
	{
	    str_append_raw(renderb, " ", 1);
	    len++;
	}
    }

    str_append_raw(renderb, "\x1b[m", 3);
    str_append_raw(renderb, "\r\n", 2);
}

void
editor_draw_status_message(const struct buffer* buffer, struct str* renderb)
{
    str_append_raw(renderb, "\x1b[K", 3);
    int meslen = strlen(buffer->status_message);

    if (meslen > config.screencols){
	meslen = config.screencols;
    }

    if (meslen && time(NULL) - buffer->status_message_time < 5)
    {
	str_append_raw(renderb, buffer->status_message, meslen);	
    }
}

void
editor_draw_rows(const struct buffer* buffer, struct str* renderb)
{
    for(int y=0; y<config.screenrows; y++)
    {
	int filerow = y + buffer->cp.rowoff;
	if (filerow >= buffer->num_rows) {
	    if (buffer->num_rows == 0 && y == config.screenrows / 3)
	    {
		char welcome[80];
		int welcomelen = snprintf(welcome, sizeof(welcome),
					  "4me version %s", FORME_VERSION);
		if (welcomelen > config.screencols)
		{
		    welcomelen = config.screencols;
		}

		int padding = (config.screencols - welcomelen) / 2;
		if (padding)
		{
		    str_append_raw(renderb, "~", 1);
		    padding--;
		}

		while (padding--)
		{
		    str_append_raw(renderb, " ", 1);
		}

		str_append_raw(renderb, welcome, welcomelen);
	    } else {
		str_append_raw(renderb, "~", 1);	    
	    }    
	}
	else
	{
	    struct row row = buffer->row[filerow];
	    int len = row.rsize - buffer->cp.coloff;
	    if (len < 0)
	    {
		len = 0;
	    }
	    if (len > config.screencols)
	    {
		len = config.screencols;
	    }
	    
	    str_append_raw(renderb, &row.render[buffer->cp.coloff], len);
	}
	
	str_append_raw(renderb, "\x1b[K", 3);
	str_append_raw(renderb, "\r\n", 2);
    }
}


void
editor_init()
{
    enable_raw_mode();
    get_window_size();
    config.screenrows -= 2;
}

struct buffer current_buffer = BUFFER_INIT;

void
editor_set_status_message(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(current_buffer.status_message, sizeof(current_buffer.status_message), fmt, ap);
    va_end(ap);
    current_buffer.status_message_time = time(NULL);
}

struct str
editor_buffer_to_string(const struct buffer* buffer)
{
    struct str res = STR_INIT;

    for (int i =0; i < buffer->num_rows; i++)
    {
	res.size += buffer->row[i].size + 1;
    }

    res.data = malloc(res.size * sizeof(char));
    char* it = res.data;
    for (int i =0; i< buffer->num_rows; i++)
    {
	memcpy(it, buffer->row[i].data, buffer->row[i].size);
	it += buffer->row[i].size;
	*it = '\n';
	it++;
    }
    
    return res;
}

void
editor_save(struct buffer* buffer)
{
    if (buffer->file_name == NULL)
    {
	return;
    }

    struct str buffer_str = editor_buffer_to_string(buffer);

    int fd = open(buffer->file_name, O_RDWR | O_CREAT, 0644);
    if (fd != -1)
    {
	if (ftruncate(fd, buffer_str.size) != -1)
	{
	    if (write(fd, buffer_str.data, buffer_str.size) == buffer_str.size)
	    {
		close(fd);
		editor_set_status_message("%d bytes has been saved to disk", buffer_str.size);
		str_deinit(&buffer_str);	
		buffer->dirty = 0;
		return;
	    }
	}
	close(fd);
    }

    str_deinit(&buffer_str);
    editor_set_status_message("Error: Can't save file %s", strerror(errno));
}

void
editor_open(const char* file_name)
{
    free(current_buffer.file_name);
    current_buffer.file_name = strdup(file_name);
    
    load_file(&current_buffer, file_name);
    current_buffer.dirty = 0;
}

void
editor_draw_update()
{
    buffer_scroll_update(&current_buffer);
    
    struct str renderb = STR_INIT;

    str_append_raw(&renderb, "\x1b[?25l", 6);
    str_append_raw(&renderb, "\x1b[H", 3);

    editor_draw_rows(&current_buffer, &renderb);
    editor_draw_status_bar(&current_buffer, &renderb);
    editor_draw_status_message(&current_buffer, &renderb);
    editor_set_cursor_position(&current_buffer, &renderb);
    
    str_append_raw(&renderb, "\x1b[?25h", 6);

    render_flush(renderb);
    str_deinit(&renderb);
}

static
void
editor_insert_char(struct buffer* buffer, int c)
{
    if (buffer->cp.y == buffer->num_rows)
    {
	buffer_append_row(buffer, "", 0);
    }

    buffer_insert_char(buffer, buffer->cp.x, c);
    buffer->cp.x++;
}

static
void
editor_remove_char(struct buffer* buffer)
{
    if (buffer->cp.y == buffer->num_rows)
    {
	return;
    }

    if (buffer->cp.x == 0 && buffer->cp.y == 0)
    {
	return;
    }

    struct row *row = buffer->row + buffer->cp.y;
    
    if (buffer->cp.x > 0)
    {
	buffer_remove_char(buffer, buffer->cp.x - 1);
	buffer->cp.x--;
    }
    else
    {
	buffer->cp.x = buffer->row[buffer->cp.y - 1].size;
	row_append_string(&buffer->row[buffer->cp.y - 1], row->data, row->size);
	buffer_del_row(buffer, buffer->cp.y);
	buffer->cp.y--;
	buffer->dirty++;
    }
}

static
void
editor_move_cursor(struct buffer* buffer, int key)
{
    struct row* row = (buffer->cp.y >= buffer->num_rows) ? NULL : &buffer->row[buffer->cp.y]; 
    
    switch (key)
    {
    case ARROW_LEFT:
	if (buffer->cp.x != 0)
	{
	    buffer->cp.x--;	    
	}
	else if (buffer->cp.y > 0)
	{
	    buffer->cp.y--;
	    buffer->cp.x = buffer->row[buffer->cp.y].size;
	}
	break;
    case ARROW_RIGHT:
	if (row && buffer->cp.x < row->size)
	{
	    buffer->cp.x++;	    	    
	}
	else if (row && buffer->cp.x == row->size)
	{
	    buffer->cp.y++;
	    buffer->cp.x = 0;
	}
	break;
    case ARROW_UP:
	if (buffer->cp.y != 0)
	{
	    buffer->cp.y--;	    
	}
	break;
    case ARROW_DOWN:
	if (buffer->cp.y < buffer->num_rows)
	{
	    buffer->cp.y++;	    
	}
	break;
    }

    row = (buffer->cp.y >= buffer->num_rows) ? NULL : &buffer->row[buffer->cp.y];
    int rowlen = row ? row->size : 0;
    if (buffer->cp.x > rowlen)
    {
	buffer->cp.x = rowlen;
    }
}

void
editor_input_update()
{
    static int quit_times = FORME_QUIT_TIMES;
    
    int c = editor_read_key();
    switch(c)
    {
    case CTRL_KEY('o'):
	editor_open("../txt");
	return;
	
    case CTRL_KEY('l'):
    case '\x1b':
	break;

    case CTRL_KEY('h'):
    case BACKSPACE:
    case DEL_KEY:
	if (c == DEL_KEY)
	{
	    editor_move_cursor(&current_buffer, ARROW_RIGHT);
	}
	editor_remove_char(&current_buffer);
	break;

    case '\r':
	break;

    case CTRL_KEY('s'):
	editor_save(&current_buffer);
	break;

    case CTRL_KEY('q'):
	if (current_buffer.dirty > 0 && quit_times > 0)
	{
	    editor_set_status_message("File has been changed. "
				      "Please press Ctrl-Q %d to quit without saving", quit_times);
	    quit_times--;
	    return;
	}

	restore();
	exit(0);
	break;
	
    case ARROW_UP:
    case ARROW_LEFT:
    case ARROW_RIGHT:
    case ARROW_DOWN:
	editor_move_cursor(&current_buffer, c);
	break;

    case HOME_KEY:
	current_buffer.cp.x = 0;
	break;
    case END_KEY:
	if (current_buffer.cp.y < current_buffer.num_rows)
	{
	    current_buffer.cp.x = current_buffer.row[current_buffer.cp.y].size;	    
	}
	break;
	
    case PAGE_UP:
    case PAGE_DOWN:
    {
	if (c == PAGE_UP)
	{
	    current_buffer.cp.y = current_buffer.cp.rowoff;
	}
	else if (c == PAGE_DOWN)
	{
	    current_buffer.cp.y = current_buffer.cp.rowoff + config.screenrows - 1;
	    if (current_buffer.cp.y > current_buffer.num_rows)
	    {
		current_buffer.cp.y = current_buffer.num_rows;
	    }
	}

	int times = config.screenrows;
	while(times--)
	{
	    editor_move_cursor(&current_buffer, c == PAGE_UP? ARROW_UP: ARROW_DOWN);
	}
    }
    break;
    default:
	editor_insert_char(&current_buffer,c);
    }

    quit_times = FORME_QUIT_TIMES;
}
