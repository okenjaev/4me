#include "str.h"

#define FOR_ME_MEMORY_ALLOCATION_FACTOR 1.5

/* str buf */

struct str_buf
str_buf_init(fint32 capacity)
{
    struct str_buf value;
    value.capacity = capacity;
    value.size = 0;
    value.data = malloc(sizeof(fchar) * value.capacity);
    return value;
}

struct str_buf
str_buf_init_raw(fchar* data, fint32 size)
{
    struct str_buf value;
    value.capacity = size;
    value.size = size;
    value.data = malloc(sizeof(fchar) * value.capacity);

    memcpy(value.data, data, value.size);
    return value;
}
void
str_buf_deinit(struct str_buf* buf)
{
    free(buf->data);
    buf->data = NULL;
    buf->capacity = 0;
    buf->size = 0;	
}

void
str_buf_append(struct str_buf* str_buf, const struct str_buf str)
{
    if (str_buf->capacity < str_buf->size + str.size)
    {
	str_buf->capacity += str.size * FOR_ME_MEMORY_ALLOCATION_FACTOR;
	str_buf->data = realloc(str_buf->data, str_buf->capacity);
    }

    memcpy(str_buf->data + str_buf->size, str.data, str.size);
    str_buf->size += str.size;
}

void
str_buf_insert_fchar(struct str_buf* str_buf, fint32 at_index, fchar c)
{
    if (str_buf->capacity < str_buf->size + 1)
    {
	str_buf->capacity += 5 * FOR_ME_MEMORY_ALLOCATION_FACTOR;
	str_buf->data = realloc(str_buf->data, str_buf->capacity);  
    }

    memmove(str_buf->data + at_index + 1, str_buf->data + at_index, str_buf->size - at_index);
    str_buf->size++;
    str_buf->data[at_index] = c;
}

void
str_buf_remove_fchar(struct str_buf* str_buf, fint32 at_index)
{
    memmove(str_buf->data + at_index, str_buf->data + at_index + 1, str_buf->size - at_index);
    str_buf->size--;
}

void
str_buf_resize(struct str_buf* str_buf,
	       fint32 size)
{
    str_buf->size = size;
    
    if (str_buf->capacity < str_buf->size)
    {
	str_buf->capacity = str_buf->size;
	str_buf->data = realloc(str_buf->data, str_buf->capacity);
    }
}

struct str_buf
str_buf_split(struct str_buf* str, const struct str_buf delim)
{
    fint32 delimsize = delim.size;

    fint32 i =0;
    while(i < str->size - delimsize)
    {
	fchar* at = i + str->data;
	struct str_buf temp = cstrn(at, delimsize);

	if (str_buf_cmp(temp, delim))
	{
	    struct str_buf ret = str_buf_init_raw(str->data, i);
	    str->size = str->size - (i + delimsize);
	    memmove(str->data, i + delimsize + str->data, str->size);
	    return ret;
	}
	
	i++;
    }

    return str_buf_nan;
}

fint32
str_buf_cmp(struct str_buf str, struct str_buf ano_str)
{
    if (str.size != ano_str.size)
    {
	return 0;
    }

    if (str.data == NULL && ano_str.data == NULL)
    {
	return 1;
    }

    for (fint32 i=0; i<str.size; i++)
    {
	if (str.data[i] != ano_str.data[i])
	{
	    return 0;
	}
    }
    
    return 1;
}

struct str_buf
str_buf_copy(const struct str_buf str)
{
    struct str_buf res;
    res.size = str.size;
    res.capacity = str.capacity;
    res.data = malloc(sizeof(fchar) * res.size);
    memcpy(res.data, str.data, res.capacity);
    return res;
}

#undef FOR_ME_MEMORY_ALLOCATION_FACTOR
