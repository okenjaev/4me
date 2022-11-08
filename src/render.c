#include "render.h"
#include "str.h"
#include "buffer.h"
#include "sys.h"
#include "row.h"

extern struct config config;

void
render_set_cursor_position(const struct buffer* buffer, struct str_buf* renderb)
{
    int y = buffer->cp.y - buffer->cp.rowoff + 1;
    int x = buffer->cp.r - buffer->cp.coloff + 1;
    char cur_pos[16];
    snprintf(cur_pos, sizeof(cur_pos), "\x1b[%d;%dH", y, x);
    str_buf_append(renderb, cstrn(cur_pos, strlen(cur_pos)));
}

void
render_draw_status_bar(const struct buffer* buffer, struct str_buf* renderb)
{
    str_buf_append(renderb, cstrn("\x1b[7m", 4));

    char status[80], rstatus[80];
    int len = snprintf(status, sizeof(status), "%.20s %s",
		       buffer->file_name ? buffer->file_name : "[empty]", buffer->dirty ? "(modified)" : "");
    int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d", buffer->cp.y + 1, buffer->num_rows);
    
    if (len > config.screencols)
    {
	len = config.screencols;
    }

    str_buf_append(renderb, cstrn(status, len));
    while(len < config.screencols)
    {
	if (config.screencols - len == rlen)
	{
	    str_buf_append(renderb, cstrn(rstatus, rlen));
	    break;
	}
	else
	{
	    str_buf_insert_char(renderb, renderb->size, ' ');
	    len++;
	}
    }

    str_buf_append(renderb, cstrn("\x1b[m", 3));
    str_buf_append(renderb, cstrn("\r\n", 2));
}

void
render_draw_status_message(struct buffer* buffer, struct str_buf* renderb)
{
    str_buf_append(renderb, cstrn("\x1b[K", 3));
    int meslen = strlen(buffer->status_message);

    if (meslen > config.screencols){
	meslen = config.screencols;
    }

    if (meslen && time(NULL) - buffer->status_message_time < 5)
    {
	str_buf_append(renderb, cstrn(buffer->status_message, meslen));	
    }
}

void
render_draw_rows(const struct buffer* buffer, struct str_buf* renderb)
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
		    str_buf_insert_char(renderb, renderb->size, '~');
		    padding--;
		}

		while (padding--)
		{
		    str_buf_insert_char(renderb, renderb->size, ' ');
		}

		str_buf_append(renderb, cstrn(welcome, welcomelen));
	    } else {
		str_buf_insert_char(renderb, renderb->size, '~');
	    }    
	}
	else
	{
	    struct row *row = buffer->row + filerow;
	    int len = row->render_chars.size - buffer->cp.coloff;
	    if (len < 0)
	    {
		len = 0;
	    }
	    if (len > config.screencols)
	    {
		len = config.screencols;
	    }
	    
	    str_buf_append(renderb,
			   cstrn(row->render_chars.data + buffer->cp.coloff, len));
	}
	
	str_buf_append(renderb, cstrn("\x1b[K", 3));
	str_buf_append(renderb, cstrn("\r\n", 2));
    }
}

int
render_flush(const struct str_buf str)
{
    return write(STDOUT_FILENO, str.data, str.size);
}
