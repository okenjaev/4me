#include "editor.h"
#include "sys.h"
#include "renderb.h"
#include "input.h"

extern struct config config;

int
buffer_row_to_render_row(struct buffer_row* row, int cx)
{
    int rx = 0;
    for(int i =0; i < cx; i++)
    {
	if (row->data[i] == '\t')
	{
	    rx +=
		(NUMBER_OF_SPACES_FOR_TAB - 1) -
		(rx % NUMBER_OF_SPACES_FOR_TAB);
	}
	rx++;
    }
    return rx;
}

void
editor_scroll(struct buffer* buffer)
{
    buffer->rx = buffer->cx;

    if (buffer->cy < buffer->num_rows)
    {
	buffer->rx = buffer_row_to_render_row(&buffer->row[buffer->cy], buffer->cx);
    }

    if (buffer->cy < buffer->rowoff)
    {
	buffer->rowoff = buffer->cy;
    }

    if (buffer->cy >= buffer->rowoff + config.screenrows)
    {
	buffer->rowoff = buffer->cy - config.screenrows + 1;
    }

    if (buffer->rx < buffer->coloff) {
	buffer->coloff = buffer->rx;
    }
    
    if (buffer->rx >= buffer->coloff + config.screencols) {
	buffer->coloff = buffer->rx - config.screencols + 1;
    }
}

void
editor_move_cursor(struct buffer* buffer,int key)
{
    struct buffer_row* row = (buffer->cy >= buffer->num_rows) ? NULL : &buffer->row[buffer->cy]; 
    
    switch (key)
    {
    case ARROW_LEFT:
	if (buffer->cx != 0)
	{
	    buffer->cx--;	    
	}
	else if (buffer->cy > 0)
	{
	    buffer->cy--;
	    buffer->cx = buffer->row[buffer->cy].size;
	}
	break;
    case ARROW_RIGHT:
	if (row && buffer->cx < row->size)
	{
	    buffer->cx++;	    	    
	}
	else if (row && buffer->cx == row->size)
	{
	    buffer->cy++;
	    buffer->cx = 0;
	}
	break;
    case ARROW_UP:
	if (buffer->cy != 0)
	{
	    buffer->cy--;	    
	}
	break;
    case ARROW_DOWN:
	if (buffer->cy < buffer->num_rows)
	{
	    buffer->cy++;	    
	}
	break;
    }

    row = (buffer->cy >= buffer->num_rows) ? NULL : &buffer->row[buffer->cy];
    int rowlen = row ? row->size : 0;
    if (buffer->cx > rowlen)
    {
	buffer->cx = rowlen;
    }
}

void
editor_set_cursor_position(const struct buffer* buffer, struct renderb* renderb)
{
    int y = buffer->cy - buffer->rowoff + 1;
    int x = buffer->rx - buffer->coloff + 1;
    char cur_pos[32];
    snprintf(cur_pos, sizeof(cur_pos), "\x1b[%d;%dH", y, x);
    renderb_append(renderb, cur_pos, strlen(cur_pos));
}

void
editor_draw_status_bar(const struct buffer* buffer, struct renderb* renderb)
{
    renderb_append(renderb, "\x1b[7m", 4);

    char status[80], rstatus[80];
    int len = snprintf(status, sizeof(status), "%.20s",
		       buffer->file_name ? buffer->file_name : "[empty]", buffer->num_rows, buffer->cy);
    int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d", buffer->cy + 1, buffer->num_rows);

    if (len > config.screencols)
    {
	len = config.screencols;
    }

    renderb_append(renderb, status, len);
    while(len < config.screencols)
    {
	if (config.screencols - len == rlen)
	{
	    renderb_append(renderb, rstatus, rlen);
	    break;
	}
	else
	{
	    renderb_append(renderb, " ", 1);
	    len++;
	}
    }

    renderb_append(renderb, "\x1b[m", 3);
    renderb_append(renderb, "\r\n", 2);
}

void editor_draw_status_message(const struct buffer* buffer, struct renderb* renderb)
{
    renderb_append(renderb, "\x1b[K", 3);
    int meslen = strlen(buffer->status_message);

    if (meslen > config.screencols){
	meslen = config.screencols;
    }

    if (meslen && time(NULL) - buffer->status_message_time < 5)
    {
	renderb_append(renderb, buffer->status_message, meslen);	
    }
}

void
editor_draw_rows(const struct buffer* buffer, struct renderb* renderb)
{
    for(int y=0; y<config.screenrows; y++)
    {
	int filerow = y + buffer->rowoff;
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
		    renderb_append(renderb, "~", 1);
		    padding--;
		}

		while (padding--)
		{
		    renderb_append(renderb, " ", 1);
		}

		renderb_append(renderb, welcome, welcomelen);
	    } else {
		renderb_append(renderb, "~", 1);	    
	    }    
	}
	else
	{
	    struct buffer_row row = buffer->row[filerow];
	    int len = row.rsize - buffer->coloff;
	    if (len < 0)
	    {
		len = 0;
	    }
	    if (len > config.screencols)
	    {
		len = config.screencols;
	    }
	    
	    renderb_append(renderb, &row.render[buffer->coloff], len);
	}
	
	renderb_append(renderb, "\x1b[K", 3);
	renderb_append(renderb, "\r\n", 2);
    }
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

void
editor_open(const char* file_name)
{
    free(current_buffer.file_name);
    current_buffer.file_name = strdup(file_name);

    load_file(&current_buffer, file_name);
}

void
editor_init()
{
    enable_raw_mode();
    get_window_size();

    config.screenrows -= 2;
}

void
editor_refresh_screen()
{
    editor_scroll(&current_buffer);
    
    struct renderb renderb = RENDERB_INIT;

    renderb_append(&renderb, "\x1b[?25l", 6);
    renderb_append(&renderb, "\x1b[H", 3);

    editor_draw_rows(&current_buffer, &renderb);
    editor_draw_status_bar(&current_buffer, &renderb);
    editor_draw_status_message(&current_buffer, &renderb);
    editor_set_cursor_position(&current_buffer, &renderb);
    
    renderb_append(&renderb, "\x1b[?25h", 6);

    renderb_flush(&renderb);
    renderb_free(&renderb);
}

void
editor_process_keys()
{
    int c = editor_read_key();
    switch(c)
    {
    case CTRL_KEY('q'):
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
	current_buffer.cx = 0;
	break;
    case END_KEY:
	if (current_buffer.cy < current_buffer.num_rows)
	{
	    current_buffer.cx = current_buffer.row[current_buffer.cy].size;	    
	}
	break;
	
    case PAGE_UP:
    case PAGE_DOWN:
    {
	if (c == PAGE_UP)
	{
	    current_buffer.cy = current_buffer.rowoff;
	}
	else if (c == PAGE_DOWN)
	{
	    current_buffer.cy = current_buffer.rowoff + config.screenrows - 1;
	    if (current_buffer.cy > current_buffer.num_rows)
	    {
		current_buffer.cy = current_buffer.num_rows;
	    }
	}

	int times = config.screenrows;
	while(times--)
	{
	    editor_move_cursor(&current_buffer, c == PAGE_UP? ARROW_UP: ARROW_DOWN);
	}
    }
    break;
    }
}
