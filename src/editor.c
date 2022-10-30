#include "editor.h"
#include "render.h"

enum editor_keys
{
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    PAGE_UP,
    PAGE_DOWN,
    HOME_KEY,
    END_KEY,
    DEL_KEY
};

int
editor_row_to_render_row(struct editor_row* row, int cx)
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
editor_scroll()
{
    e.rx = e.cx;

    if (e.cy < e.num_rows)
    {
	e.rx = editor_row_to_render_row(&e.row[e.cy], e.cx);
    }

    if (e.cy < e.rowoff)
    {
	e.rowoff = e.cy;
    }

    if (e.cy >= e.rowoff + e.screenrows)
    {
	e.rowoff = e.cy - e.screenrows + 1;
    }

    if (e.rx < e.coloff) {
	e.coloff = e.rx;
    }
    
    if (e.rx >= e.coloff + e.screencols) {
	e.coloff = e.rx - e.screencols + 1;
    }
}

void
editor_update_row(struct editor_row* row)
{
    int tabs = 0;

    for (int j =0; j < row->size; j++)
    {
	if (row->data[j] == '\t')
	{
	    tabs++;
	}
    }
    
    free(row->render);
    row->render = malloc(row->size + tabs * (NUMBER_OF_SPACES_FOR_TAB - 1) + 1);

    int idx = 0;
    for (int j = 0; j < row->size; j++)
    {
	if (row->data[j] == '\t')
	{
	    row->render[idx++] = ' ';
	    while(idx % NUMBER_OF_SPACES_FOR_TAB != 0)
	    {
		row->render[idx++] = ' ';
	    }
	}
	else
	{
	    row->render[idx++] = row->data[j];
	}
    }

    row->render[idx] = '\0';
    row->rsize = idx;
}

void editor_append_row(char* string, int len)
{
    e.row = realloc(e.row, sizeof(struct editor_row) * (e.num_rows + 1));

    struct editor_row *at = &e.row[e.num_rows];
    
    at->size = len;
    at->data = malloc(len + 1);
    memcpy(at->data, string, len);
    at->data[len] = '\0';
    at->rsize = 0;
    at->render = NULL;

    editor_update_row(at);

    e.num_rows++;
}

void
editor_move_cursor(int key)
{
    struct editor_row* row = (e.cy >= e.num_rows) ? NULL : &e.row[e.cy]; 
    
    switch (key)
    {
    case ARROW_LEFT:
	if (e.cx != 0)
	{
	    e.cx--;	    
	}
	else if (e.cy > 0)
	{
	    e.cy--;
	    e.cx = e.row[e.cy].size;
	}
	break;
    case ARROW_RIGHT:
	if (row && e.cx < row->size)
	{
	    e.cx++;	    	    
	}
	else if (row && e.cx == row->size)
	{
	    e.cy++;
	    e.cx = 0;
	}
	break;
    case ARROW_UP:
	if (e.cy != 0)
	{
	    e.cy--;	    
	}
	break;
    case ARROW_DOWN:
	if (e.cy < e.num_rows)
	{
	    e.cy++;	    
	}
	break;
    }

    row = (e.cy >= e.num_rows) ? NULL : &e.row[e.cy];
    int rowlen = row ? row->size : 0;
    if (e.cx > rowlen)
    {
	e.cx = rowlen;
    }
}

void
editor_set_status_message(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(e.status_message, sizeof(e.status_message), fmt, ap);
    va_end(ap);
    e.status_message_time = time(NULL);
}

void
editor_set_cursor_position(struct buffer* buffer)
{
    int y = e.cy - e.rowoff + 1;
    int x = e.rx - e.coloff + 1;
    char cur_pos[32];
    snprintf(cur_pos, sizeof(cur_pos), "\x1b[%d;%dH", y, x);
    buffer_append(buffer, cur_pos, strlen(cur_pos));
}

void
editor_draw_status_bar(struct buffer* buffer)
{
    buffer_append(buffer, "\x1b[7m", 4);

    char status[80], rstatus[80];
    int len = snprintf(status, sizeof(status), "%.20s",
		       e.file_name ? e.file_name : "[empty]", e.num_rows, e.cy);
    int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d", e.cy + 1, e.num_rows);

    if (len > e.screencols)
    {
	len = e.screencols;
    }

    buffer_append(buffer, status, len);
    while(len < e.screencols)
    {
	if (e.screencols - len == rlen)
	{
	    buffer_append(buffer, rstatus, rlen);
	    break;
	}
	else
	{
	    buffer_append(buffer, " ", 1);
	    len++;
	}
    }

    buffer_append(buffer, "\x1b[m", 3);
    buffer_append(buffer, "\r\n", 2);
}

void editor_draw_status_message(struct buffer* buffer)
{
    buffer_append(buffer, "\x1b[K", 3);
    int meslen = strlen(e.status_message);

    if (meslen > e.screencols){
	meslen = e.screencols;
    }

    if (meslen && time(NULL) - e.status_message_time < 5)
    {
	buffer_append(buffer, e.status_message, meslen);	
    }
}

void
editor_draw_rows(struct buffer* buffer)
{
    for(int y=0; y<e.screenrows; y++)
    {
	int filerow = y + e.rowoff;
	if (filerow >= e.num_rows) {
	    if (e.num_rows == 0 && y == e.screenrows / 3)
	    {
		char welcome[80];
		int welcomelen = snprintf(welcome, sizeof(welcome),
					  "4me version %s", FORME_VERSION);
		if (welcomelen > e.screencols)
		{
		    welcomelen = e.screencols;
		}

		int padding = (e.screencols - welcomelen) / 2;
		if (padding)
		{
		    buffer_append(buffer, "~", 1);
		    padding--;
		}

		while (padding--)
		{
		    buffer_append(buffer, " ", 1);
		}

		buffer_append(buffer, welcome, welcomelen);
	    } else {
		buffer_append(buffer, "~", 1);	    
	    }    
	}
	else
	{
	    struct editor_row row = e.row[filerow];
	    int len = row.rsize - e.coloff;
	    if (len < 0)
	    {
		len = 0;
	    }
	    if (len > e.screencols)
	    {
		len = e.screencols;
	    }
	    
	    buffer_append(buffer, &row.render[e.coloff], len);
	}
	
	buffer_append(buffer, "\x1b[K", 3);
	buffer_append(buffer, "\r\n", 2);
    }
}

void
editor_open(const char* file_name)
{
    free(e.file_name);
    e.file_name = strdup(file_name);
    
    FILE *fp = fopen(file_name, "r");
    if(!fp)
    {
	die("file_open");
    }

    char* line = NULL;
    size_t linecap = 0;
    ssize_t linelen = 0;

    while ((linelen = getline(&line, &linecap, fp)) != -1)
    {
	
	while(linelen > 0 && (line[linelen - 1] == '\n' ||
			      line[linelen-1] == '\r'))
	{
	    linelen--;
	}
	editor_append_row(line, linelen);
    }

    free(line);
    fclose(fp);
}

void
editor_init()
{
    term_init(&e);
    
    if (term_get_window_size(&e.screenrows, &e.screencols) == -1)
    {
        die("getwindowsize");
    }

    e.screenrows -= 2;
}

void
editor_refresh_screen()
{
    editor_scroll();
    
    struct buffer buffer = BUFFER_INIT;

    buffer_append(&buffer, "\x1b[?25l", 6);
    buffer_append(&buffer, "\x1b[H", 3);

    editor_draw_rows(&buffer);
    editor_draw_status_bar(&buffer);
    editor_draw_status_message(&buffer);
    editor_set_cursor_position(&buffer);
    
    buffer_append(&buffer, "\x1b[?25h", 6);

    render(&buffer);
    buffer_free(&buffer);
}

int
editor_read_key()
{
    int nread;
    char c;
    while((nread = read(STDIN_FILENO, &c, 1)) != 1)
    {
	if (nread == -1 && errno != EAGAIN)
	{
	    die("read");
	}
    }

    if (c == '\x1b')
    {
	char seq[3];

	if (read(STDIN_FILENO, &seq[0], 1) != 1)
	{
	    return '\x1b';
	}

	if (read(STDIN_FILENO, &seq[1], 1) != 1)
	{
	    return '\x1b';
	}

	if (seq[0] == '[')
	{
	    if (seq[1] >= '0' && seq[1] <= '9')
	    {
		if (read(STDIN_FILENO, &seq[2], 1) != 1)
		{
		    return '\x1b';
		}

		if (seq[2] == '~')
		{
		    switch(seq[1])
		    {
		    case '1':
			return HOME_KEY;
		    case '3':
			return DEL_KEY;
		    case '4':
			return END_KEY;
		    case '5':
			return PAGE_UP;
		    case '6':
			return PAGE_DOWN;
		    case '7':
			return HOME_KEY;
		    case '8':
			return END_KEY;
		    }
		}
	    }
	    else
	    {
		switch(seq[1])
		{
		case 'A':
		    return ARROW_UP;
		case 'B':
		    return ARROW_DOWN;
		case 'C':
		    return ARROW_RIGHT;
		case 'D':
		    return ARROW_LEFT;
		case 'H':
		    return HOME_KEY;
		case 'F':
		    return END_KEY;
		}
	    }
	}
	else if (seq[0] == 'O')
	{
	    switch(seq[1])
	    {
	    case 'H':
		return HOME_KEY;
	    case 'F':
		return END_KEY;
	    }
	}
	return '\x1b';
    }
    else
    {
	return c;	
    }    
}

void
editor_process_keys()
{
    int c = editor_read_key();
    switch(c)
    {
    case CTRL_KEY('q'):
	struct buffer buffer = BUFFER_INIT;
	buffer_append(&buffer, "\x1b[2J", 4);
        buffer_append(&buffer, "\x1b[H", 3);
	render(&buffer);
	buffer_free(&buffer);
	exit(0);
	break;
    case ARROW_UP:
    case ARROW_LEFT:
    case ARROW_RIGHT:
    case ARROW_DOWN:
	editor_move_cursor(c);
	break;

    case HOME_KEY:
	e.cx = 0;
	break;
    case END_KEY:
	if (e.cy < e.num_rows)
	{
	    e.cx = e.row[e.cy].size;	    
	}
	break;
	
    case PAGE_UP:
    case PAGE_DOWN:
    {
	if (c == PAGE_UP)
	{
	    e.cy = e.rowoff;
	}
	else if (c == PAGE_DOWN)
	{
	    e.cy = e.rowoff + e.screenrows - 1;
	    if (e.cy > e.num_rows)
	    {
		e.cy = e.num_rows;
	    }
	}

	int times = e.screenrows;
	while(times--)
	{
	    editor_move_cursor(c == PAGE_UP? ARROW_UP: ARROW_DOWN);
	}
    }
    break;
    }
}