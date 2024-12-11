#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <pthread.h>
#include <immintrin.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int g_msg_len = 0;
int g_start_x = 0;
int g_start_y = 0;

typedef char i8;
typedef unsigned char u8;
typedef unsigned short u16;
typedef int i32;
typedef unsigned u32;
typedef unsigned long u64;

#define PRINT_ERROR(cstring) write(STDERR_FILENO, cstring, sizeof(cstring) - 1)

#define true 1
#define false 0

#pragma pack(1)
struct bmp_header
{
	// Note: header
	i8  signature[2]; // should equal to "BM"
	u32 file_size;
	u32 unused_0;
	u32 data_offset;

	// Note: info header
	u32 info_header_size;
	u32 width; // in px
	u32 height; // in px
	u16 number_of_planes; // should be 1
	u16 bit_per_pixel; // 1, 4, 8, 16, 24 or 32
	u32 compression_type; // should be 0
	u32 compressed_image_size; // should be 0
	// Note: there are more stuff there but it is not important here
};

struct file_content
{
	i8*   data;
	u32   size;
};

struct file_content   read_entire_file(char* filename)
{
	char* file_data = 0;
	unsigned long	file_size = 0;
	int input_file_fd = open(filename, O_RDONLY);
	if (input_file_fd >= 0)
	{
		struct stat input_file_stat = {0};
		stat(filename, &input_file_stat);
		file_size = input_file_stat.st_size;
		file_data = mmap(0, file_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, input_file_fd, 0);
		close(input_file_fd);
	}
	return (struct file_content){file_data, file_size};
}

int check_header(struct bmp_header* header, u8* pixel_data, u32 x, u32 y)
{
	u8* pixel;
	u32 temp = x;
	u32 row_width = header->width * 4;
	u8* base_row = pixel_data + ((header->height - 1 - y) * row_width);
	for (u32 i = x; i < 7; i++)
	{
		// pixel = pixel_data + ((header->height - 1 - y) * header->width + i) * 4;
		pixel = base_row + (i * 4);
		if (pixel[0] != 127 || pixel[1] != 188 || pixel[2] != 217)
			return (false);
	}
	pixel = pixel_data + ((header->height - 1 - y) * header->width + x + 7) * 4;
	pixel = base_row + ((x + 7) * 4);
	if (pixel[0] == 127 && pixel[1] == 188 && pixel[2] == 217)
		return (false);
	else
	{
		// pixel = pixel_data + ((header->height - 1 - y) * header->width + x + 7) * 4;
		pixel = base_row + ((x + 7) * 4);
		// printf("found pixel at (%u, %u)\n", x + 7, header->height - 1 - y);
		// printf("b: %d, r: %d\n", pixel[0], pixel[2]);
		g_msg_len = pixel[0] + pixel[2];
		if (g_msg_len > 510)
			return (false);
	};

	for (u32 i = y; i < 7; i++)
	{
		// pixel = pixel_data + ((header->height - 1 - i) * header->width + temp) * 4;
		pixel = base_row + (temp * 4);
		if (pixel[0] != 127 || pixel[1] != 188 || pixel[2] != 217)
			return (false);
	}
	g_start_x = x;
	g_start_y = y;
	return (true);
}

void	printing_msg(struct bmp_header* header, u8* pixel_data)
{
	int len = g_msg_len;
	int i = 0;
	
	g_start_x += 2;
	int x = g_start_x;
	int y = g_start_y + 2;

	char * output = malloc(len + 1);
	while (i < len)
	{
		u8* msg_pixel = pixel_data + ((header->height - 1 - y) * header->width + x) * 4;
		output[i++] = msg_pixel[0];
		if (i < len)
			output[i++] = msg_pixel[1];
		if (i < len)
			output[i++] = msg_pixel[2];

		x++;
		if (g_start_x + 6 == x)
		{
			x = g_start_x;
			y++;
		}
	}
	output[i] = '\0';
	write (STDOUT_FILENO, output, len);
	free(output);
}

void	find_start(struct bmp_header* header, u8* pixel_data, u8* pixel, u32 x, u32 *y)
{
	while (pixel[0] == 127 && pixel[1] == 188 && pixel[2] == 217)
	{
		(*y)--;
		pixel = pixel_data + ((header->height - 1 - *y) * header->width + x) * 4;
	}
	(*y)++;
}
int main(int argc, char** argv)
{
	// clock_t start_time = clock();
	if (argc != 2)
	{
		PRINT_ERROR("Usage: decode <input_filename>\n");
		return 1;
	}
	struct file_content file_content = read_entire_file(argv[1]);
	if (file_content.data == NULL)
	{
		PRINT_ERROR("Failed to read file\n");
		return 1;
	}
	struct bmp_header* header = (struct bmp_header*) file_content.data;

	u8* pixel_data = (u8*)(file_content.data + header->data_offset);
	u8* pixel;

	for (u32 y = 0; y < header->height; y+=7)
	{
		for (u32 x = 0; x < header->width; x++)
		{
			pixel = pixel_data + ((header->height - 1 - y) * header->width + x) * 4;

			if (pixel[0] == 127 && pixel[1] == 188 && pixel[2] == 217)
			{
				find_start(header, pixel_data, pixel, x, &y);
				if (check_header(header, pixel_data, x, y) == true)
				{
					printing_msg(header, pixel_data);
					// clock_t end_time = clock();
					// double elapsed_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
					// printf("\nElapsed time: %f seconds\n", elapsed_time);

					return (0);
				}
			}
    	}
	}
	return (0);
}
