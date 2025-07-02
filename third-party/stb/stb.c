
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

typedef struct gif_result_t {
	int delay;
	unsigned char *data;
	struct gif_result_t *next;
} gif_result;

STBIDEF unsigned char *stbi_xload(char const *filename, int *x, int *y, int *frames)//load gif animated
{
	FILE *f;
	stbi__context s;
	unsigned char *result = 0;

	if (!(f = stbi__fopen(filename, "rb")))
		return stbi__errpuc("can't fopen", "Unable to open file");

	stbi__start_file(&s, f);

	if (stbi__gif_test(&s))
	{
		int c;
		stbi__gif g;
		gif_result head;
		gif_result *prev = 0, *gr = &head;

		memset(&g, 0, sizeof(g));
		memset(&head, 0, sizeof(head));

		*frames = 0;

		while ((gr->data = stbi__gif_load_next(&s, &g, &c, 4)) != NULL)
		{
			if (gr->data == (unsigned char*)&s)
			{
				gr->data = 0;
				break;
			}

			if (prev) prev->next = gr;
			gr->delay = g.delay;
			prev = gr;
			gr = (gif_result*) stbi__malloc(sizeof(gif_result));
			memset(gr, 0, sizeof(gif_result));
			++(*frames);
		}

		STBI_FREE(g.out);

		if (gr != &head)
			STBI_FREE(gr);

		if (*frames > 0)
		{
			*x = g.w;
			*y = g.h;
		}

		result = head.data;

		if (*frames > 1)
		{
			unsigned int size = 4 * g.w * g.h;
			unsigned char *p = 0;

			result = (unsigned char*)stbi__malloc(*frames * (size + 2));
			gr = &head;
			p = result;

			while (gr)
			{
				prev = gr;
				memcpy(p, gr->data, size);
				p += size;
				*p++ = gr->delay & 0xFF;
				*p++ = (gr->delay & 0xFF00) >> 8;
				gr = gr->next;

				STBI_FREE(prev->data);
				if (prev != &head) STBI_FREE(prev);
			}
		}
	}
	else
	{
		result = stbi__load_main(&s, x, y, frames, 4);
		*frames = !!result;
	}

	fclose(f);
	return result;
}
//#include "stb.c"
