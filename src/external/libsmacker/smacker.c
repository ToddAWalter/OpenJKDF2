/**
	libsmacker - A C library for decoding .smk Smacker Video files
	Copyright (C) 2012-2017 Greg Kennedy

	See smacker.h for more information.

	smacker.c
		Main implementation file of libsmacker.
		Open, close, query, render, advance and seek an smk
*/

#include "smacker.h"

/* safe malloc and free */
#include "smk_malloc.h"

/* data structures */
#include "smk_bitstream.h"
#include "smk_hufftree.h"

/* GLOBALS */
/* tree processing order */
#define SMK_TREE_MMAP	0
#define SMK_TREE_MCLR	1
#define SMK_TREE_FULL	2
#define SMK_TREE_TYPE	3

/* SMACKER DATA STRUCTURES */
struct smk_t
{
	/* meta-info */
	/* file mode: see flags, smacker.h */
	uint8_t	mode;

	/* microsec per frame - stored as a double to handle scaling
		(large positive millisec / frame values may overflow a ul) */
	double	usf;

	/* total frames */
	uint32_t	f;
	/* does file have a ring frame? (in other words, does file loop?) */
	uint8_t	ring_frame;

	/* Index of current frame */
	uint32_t	cur_frame;

	/* SOURCE union.
		Where the data is going to be read from (or be stored),
		depending on the file mode. */
	union
	{
		struct
		{
			/* on-disk mode */
			FILE* fp;
			uint32_t* chunk_offset;
		} file;

		/* in-memory mode: unprocessed chunks */
		uint8_t** chunk_data;
	} source;

	/* shared array of "chunk sizes"*/
	uint32_t* chunk_size;

	/* Holds per-frame flags (i.e. 'keyframe') */
	uint8_t* keyframe;
	/* Holds per-frame type mask (e.g. 'audio track 3, 2, and palette swap') */
	uint8_t* frame_type;

	/* video and audio structures */
	/* Video data type: enable/disable decode switch,
		video info and flags,
		pointer to last-decoded-palette */
	struct smk_video_t
	{
		/* enable/disable decode switch */
		uint8_t enable;

		/* video info */
		uint32_t	w;
		uint32_t	h;
		/* Y scale mode (constants defined in smacker.h)
			0: unscaled
			1: doubled
			2: interlaced */
		uint8_t	y_scale_mode;

		/* version ('2' or '4') */
		uint8_t	v;

		/* Huffman trees */
		/* uint32_t tree_size[4]; */
		struct smk_huff16_t* tree[4];

		/* Palette data type: pointer to last-decoded-palette */
		uint8_t palette[256][3];
		/* Last-unpacked frame */
		uint8_t* frame;
		struct smk_bit_t bs;
	} video;

	/* audio structure */
	struct smk_audio_t
	{
		/* set if track exists in file */
		uint8_t exists;

		/* enable/disable switch (per track) */
		uint8_t enable;

		/* Info */
		uint8_t	channels;
		uint8_t	bitdepth;
		uint32_t	rate;
		int32_t	max_buffer;

		/* compression type
			0: raw PCM
			1: SMK DPCM
			2: Bink (Perceptual), unsupported */
		uint8_t	compress;

		/* pointer to last-decoded-audio-buffer */
		void* buffer;
		uint32_t	buffer_size;
		struct smk_bit_t bs;
	} audio[7];
};

union smk_read_t
{
	FILE* file;
	uint8_t* ram;
};

/* An fread wrapper: consumes N bytes, or returns -1
	on failure (when size doesn't match expected) */
static char smk_read_file(void* buf, const size_t size, FILE* fp)
{
	/* don't bother checking buf or fp, fread does it for us */
	size_t bytesRead = fread(buf,1,size,fp);
	if (bytesRead != size)
	{
		fprintf(stderr, "libsmacker::smk_read_file(buf,%lu,fp) - ERROR: int16_t read, %lu bytes returned\n\tReason: %s\n", (uint32_t)size, (uint32_t)bytesRead, strerror(errno));
		return -1;
	}
	return 0;
}

/* A memcpy wrapper: consumes N bytes, or returns -1
	on failure (when size too low) */
static char smk_read_memory(void* buf, const uint32_t size, uint8_t** p, uint32_t* p_size)
{
	if (size > *p_size)
	{
		fprintf(stderr,"libsmacker::smk_read_memory(buf,%lu,p,%lu) - ERROR: int16_t read\n",(uint32_t)size, (uint32_t)*p_size);
		return -1;
	}
	memcpy(buf,*p,size);
	*p += size;
	*p_size -= size;
	return 0;
}

/* Helper functions to do the reading, plus
	byteswap from LE to host order */
/* read n bytes from (source) into ret */
#define smk_read(ret,n) \
{ \
	if (m) \
	{ \
		r = (smk_read_file(ret,n,fp.file)); \
	} \
	else \
	{ \
		r = (smk_read_memory(ret,n,&fp.ram,&size)); \
	} \
	if (r < 0) \
	{ \
		fprintf(stderr,"libsmacker::smk_read(...) - Errors encountered on read, bailing out (file: %s, line: %lu)\n", __FILE__, (uint32_t)__LINE__); \
		goto error; \
	} \
}

/* Calls smk_read, but returns a ul */
#define smk_read_ul(p) \
{ \
	smk_read(buf,4); \
	p = ((uint32_t) buf[3] << 24) | \
		((uint32_t) buf[2] << 16) | \
		((uint32_t) buf[1] << 8) | \
		((uint32_t) buf[0]); \
}

/* PUBLIC FUNCTIONS */
/* open an smk (from a generic Source) */
static smk smk_open_generic(const uint8_t m, union smk_read_t fp, uint32_t size, const uint8_t process_mode)
{
	smk s = NULL;
	struct smk_bit_t* bs = NULL;

	/* Temporary variables */
	int32_t temp_l;
	uint32_t temp_u;

	/* r is used by macros above for return code */
	char r;
	uint8_t buf[4] = {'\0'};

	/* video hufftrees are stored as a large chunk (bitstream)
		these vars are used to load, then decode them */
	uint8_t* hufftree_chunk = NULL;
	uint32_t tree_size;

	/* safe malloc the structure */
	smk_malloc(s,sizeof (struct smk_t));

	/* Check for a valid signature */
	smk_read(buf,3);
	if (buf[0] != 'S' || buf[1] != 'M' || buf[2] != 'K')
	{
		fprintf(stderr,"libsmacker::smk_open_generic - ERROR: invalid SMKn signature (got: %s)\n",buf);
		goto error;
	}

	/* Read .smk file version */
	smk_read(&s->video.v,1);
	if (s->video.v != '2' && s->video.v != '4')
	{
		fprintf(stderr,"libsmacker::smk_open_generic - Warning: invalid SMK version %c (expected: 2 or 4)\n",s->video.v);
		/* take a guess */
		if (s->video.v < '4')
			s->video.v = '2';
		else
			s->video.v = '4';
		fprintf(stderr,"\tProcessing will continue as type %c\n",s->video.v);
	}

	/* width, height, total num frames */
	smk_read_ul(s->video.w);
	smk_read_ul(s->video.h);

	smk_read_ul(s->f);

	/* frames per second calculation */
	smk_read_ul(temp_u);
	temp_l = (int)temp_u;
	if (temp_l > 0)
	{
		/* millisec per frame */
		s->usf = temp_l * 1000;
	}
	else if (temp_l < 0)
	{
		/* 10 microsec per frame */
		s->usf = temp_l * -10;
	}
	else
	{
		/* defaults to 10 usf (= 100000 microseconds) */
		s->usf = 100000;
	}

	/* Video flags follow.
		Ring frame is important to libsmacker.
		Y scale / Y interlace go in the Video flags.
		The user should scale appropriately. */
	smk_read_ul(temp_u);
	if (temp_u & 0x01)
	{
		s->ring_frame = 1;
	}
	if (temp_u & 0x02)
	{
		s->video.y_scale_mode = SMK_FLAG_Y_DOUBLE;
	}
	if (temp_u & 0x04)
	{
		if (s->video.y_scale_mode == SMK_FLAG_Y_DOUBLE)
		{
			fputs("libsmacker::smk_open_generic - Warning: SMK file specifies both Y-Double AND Y-Interlace.\n",stderr);
		}
		s->video.y_scale_mode = SMK_FLAG_Y_INTERLACE;
	}

	/* Max buffer size for each audio track - used to pre-allocate buffers */
	for (temp_l = 0; temp_l < 7; temp_l ++)
	{
		smk_read_ul(s->audio[temp_l].max_buffer);
	}

	/* Read size of "hufftree chunk" - save for later. */
	smk_read_ul(tree_size);

	/* "unpacked" sizes of each huff tree - we don't use
		but calling application might. */
	for (temp_l = 0; temp_l < 4; temp_l ++)
	{
/*		smk_read_ul(s->video.tree_size[temp_u]); */
		smk_read_ul(temp_u);
	}

	/* read audio rate data */
	for (temp_l = 0; temp_l < 7; temp_l ++)
	{
		smk_read_ul(temp_u);
		if (temp_u & 0x40000000)
		{
			/* Audio track specifies "exists" flag, malloc structure and copy components. */
			s->audio[temp_l].exists = 1;

			/* and for all audio tracks */
			smk_malloc(s->audio[temp_l].buffer, s->audio[temp_l].max_buffer);

			if (temp_u & 0x80000000)
			{
				s->audio[temp_l].compress = 1;
			}
			s->audio[temp_l].bitdepth = ((temp_u & 0x20000000) ? 16 : 8);
			s->audio[temp_l].channels = ((temp_u & 0x10000000) ? 2 : 1);
			if (temp_u & 0x0c000000)
			{
				fprintf(stderr,"libsmacker::smk_open_generic - Warning: audio track %ld is compressed with Bink (perceptual) Audio Codec: this is currently unsupported by libsmacker\n",temp_l);
				s->audio[temp_l].compress = 2;
			}
			/* Bits 25 & 24 are unused. */
			s->audio[temp_l].rate = (temp_u & 0x00FFFFFF);
		}
		else if (temp_u == 4)
		{
			s->audio[temp_l].exists = 1;
			smk_malloc(s->audio[temp_l].buffer, s->audio[temp_l].max_buffer);
			s->audio[temp_l].compress = 0;
			s->audio[temp_l].bitdepth = 0;
			s->audio[temp_l].channels = 0;
			s->audio[temp_l].rate = 4;
		}
	}

	/* Skip over Dummy field */
	smk_read_ul(temp_u);

	/* FrameSizes and Keyframe marker are stored together. */
	smk_malloc(s->keyframe,(s->f + s->ring_frame));
	smk_malloc(s->chunk_size,(s->f + s->ring_frame) * sizeof(uint32_t));

	for (temp_u = 0; temp_u < (s->f + s->ring_frame); temp_u ++)
	{
		smk_read_ul(s->chunk_size[temp_u]);

		/* Set Keyframe */
		if (s->chunk_size[temp_u] & 0x01)
		{
			s->keyframe[temp_u] = 1;
		}
		/* Bits 1 is used, but the purpose is unknown. */
		s->chunk_size[temp_u] &= 0xFFFFFFFC;
	}

	/* That was easy... Now read FrameTypes! */
	smk_malloc(s->frame_type,(s->f + s->ring_frame));
	for (temp_u = 0; temp_u < (s->f + s->ring_frame); temp_u ++)
	{
		smk_read(&s->frame_type[temp_u],1);
	}

	/* HuffmanTrees
		We know the sizes already: read and assemble into
		something actually parse-able at run-time */
	smk_malloc(hufftree_chunk,tree_size);
	smk_read(hufftree_chunk,tree_size);

	/* set up a Bitstream */
	bs = smk_bs_init(hufftree_chunk, tree_size);
	/* create some tables */
	for (temp_u = 0; temp_u < 4; temp_u ++)
	{
		smk_huff16_build(bs,s->video.tree[temp_u]);
	}

	/* clean up */
	smk_free(bs);
	smk_free(hufftree_chunk);

	/* Go ahead and malloc storage for the video frame */
	smk_malloc(s->video.frame,s->video.w * s->video.h);

	/* final processing: depending on ProcessMode, handle what to do with rest of file data */
	s->mode = process_mode;

	/* Handle the rest of the data.
		For MODE_MEMORY, read the chunks and store */
	if (s->mode == SMK_MODE_MEMORY)
	{
		smk_malloc(s->source.chunk_data,(s->f + s->ring_frame) * sizeof(uint8_t*));
		for (temp_u = 0; temp_u < (s->f + s->ring_frame); temp_u ++)
		{
			smk_malloc(s->source.chunk_data[temp_u],s->chunk_size[temp_u]);
			smk_read(s->source.chunk_data[temp_u],s->chunk_size[temp_u]);
		}
	}
	else
	{
		/* MODE_STREAM: don't read anything now, just precompute offsets.
			use fseek to verify that the file is "complete" */
		smk_malloc(s->source.file.chunk_offset,(s->f + s->ring_frame) * sizeof(uint32_t));
		for (temp_u = 0; temp_u < (s->f + s->ring_frame); temp_u ++)
		{
			s->source.file.chunk_offset[temp_u] = ftell(fp.file);
			if (fseek(fp.file,s->chunk_size[temp_u],SEEK_CUR))
			{
				fprintf(stderr,"libsmacker::smk_open - ERROR: fseek to frame %lu not OK.\n",temp_u);
				perror ("\tError reported was");
				goto error;
			}
		}
	}

	return s;

error:
	smk_free(bs);
	smk_free(hufftree_chunk);
	smk_close(s);
	return NULL;
}

/* open an smk (from a memory buffer) */
smk smk_open_memory(const uint8_t* buffer, const uint32_t size)
{
	smk s = NULL;

	union smk_read_t fp;

	smk_assert(buffer);

	/* set up the read union for Memory mode */
	fp.ram = (uint8_t*)buffer;

	if (!(s = smk_open_generic(0,fp,size,SMK_MODE_MEMORY)))
	{
		fprintf(stderr,"libsmacker::smk_open_memory(buffer,%lu) - ERROR: Fatal error in smk_open_generic, returning NULL.\n",size);
	}

	/* fall through, return s or null */
error:
	return s;
}

/* open an smk (from a file) */
smk smk_open_filepointer(FILE* file, const uint8_t mode)
{
	smk s = NULL;
	union smk_read_t fp;

	smk_assert(file);

	/* Copy file ptr to internal union */
	fp.file = file;

	if (!(s = smk_open_generic(1,fp,0,mode)))
	{
		fprintf(stderr,"libsmacker::smk_open_filepointer(file,%u) - ERROR: Fatal error in smk_open_generic, returning NULL.\n",mode);
		fclose(fp.file);
		goto error;
	}

	if (mode == SMK_MODE_MEMORY)
	{
		fclose(fp.file);
	}
	else
	{
		s->source.file.fp = fp.file;
	}

	/* fall through, return s or null */
error:
	return s;
}

/* open an smk (from a file) */
smk smk_open_file(const char* filename, const uint8_t mode)
{
	FILE* fp;

	smk_assert(filename);

	if (!(fp = fopen(filename,"rb")))
	{
		fprintf(stderr,"libsmacker::smk_open_file(%s,%u) - ERROR: could not open file (errno: %d)\n",filename,mode,errno);
		perror ("\tError reported was");
		goto error;
	}

	/* kick processing to smk_open_filepointer */
	return smk_open_filepointer(fp,mode);

	/* fall through, return s or null */
error:
	return NULL;
}

/* close out an smk file and clean up memory */
void smk_close(smk s)
{
	uint32_t u;

	smk_assert(s);

	/* free video sub-components */
	{
		for (u = 0; u < 4; u ++)
		{
			if (s->video.tree[u]) smk_huff16_free(s->video.tree[u]);
		}
		smk_free(s->video.frame);
	}

	/* free audio sub-components */
	for (u=0; u<7; u++)
	{
		smk_free(s->audio[u].buffer);
	}

	smk_free(s->keyframe);
	smk_free(s->frame_type);

	if (s->mode == SMK_MODE_DISK)
	{
		/* disk-mode */
		if (s->source.file.fp)
		{
			fclose(s->source.file.fp);
		}
		smk_free(s->source.file.chunk_offset);
	}
	else
	{
		/* mem-mode */
		if (s->source.chunk_data != NULL)
		{
			for (u=0; u<(s->f + s->ring_frame); u++)
			{
				smk_free(s->source.chunk_data[u]);
			}
			smk_free(s->source.chunk_data);
		}
	}
	smk_free(s->chunk_size);

	smk_free(s);

error: ;
}

/* tell some info about the file */
char smk_info_all(const smk object, uint32_t* frame, uint32_t* frame_count, double* usf)
{
	/* sanity check */
	smk_assert(object);
	if (!frame && !frame_count && !usf) {
		fputs("libsmacker::smk_info_all(object,frame,frame_count,usf) - ERROR: Request for info with all-NULL return references\n",stderr);
		goto error;
	}
	if (frame)
		*frame = (object->cur_frame % object->f);

	if (frame_count)
		*frame_count = object->f;

	if (usf)
		*usf = object->usf;

	return 0;

error:
	return -1;
}

char smk_info_video(const smk object, uint32_t* w, uint32_t* h, uint8_t* y_scale_mode)
{
	/* sanity check */
	smk_assert(object);
	if (!w && !h && !y_scale_mode)
	{
		fputs("libsmacker::smk_info_all(object,w,h,y_scale_mode) - ERROR: Request for info with all-NULL return references\n",stderr);
		return -1;
	}

	if (w)
		*w = object->video.w;

	if (h)
		*h = object->video.h;

	if (y_scale_mode)
		*y_scale_mode = object->video.y_scale_mode;

	return 0;

error:
	return -1;
}

char smk_info_audio(const smk object, uint8_t* track_mask, uint8_t channels[7], uint8_t bitdepth[7], uint32_t audio_rate[7])
{
	uint8_t i;

	/* sanity check */
	smk_assert(object);

	if (!track_mask && !channels && !bitdepth && !audio_rate)
	{
		fputs("libsmacker::smk_info_audio(object,track_mask,channels,bitdepth,audio_rate) - ERROR: Request for info with all-NULL return references\n",stderr);
		return -1;
	}
	if (track_mask)
	{
		*track_mask = ( (object->audio[0].exists) |
			 ( (object->audio[1].exists) << 1 ) |
			 ( (object->audio[2].exists) << 2 ) |
			 ( (object->audio[3].exists) << 3 ) |
			 ( (object->audio[4].exists) << 4 ) |
			 ( (object->audio[5].exists) << 5 ) |
			 ( (object->audio[6].exists) << 6 ) );
	}
	if (channels)
	{
		for (i = 0; i < 7; i ++)
		{
			channels[i] = object->audio[i].channels;
		}
	}
	if (bitdepth)
	{
		for (i = 0; i < 7; i ++)
		{
			bitdepth[i] = object->audio[i].bitdepth;
		}
	}
	if (audio_rate)
	{
		for (i = 0; i < 7; i ++)
		{
			audio_rate[i] = object->audio[i].rate;
		}
	}
	return 0;

error:
	return -1;
}

/* Enable-disable switches */
char smk_enable_all(smk object, const uint8_t mask)
{
	uint8_t i;

	/* sanity check */
	smk_assert(object);

	/* set video-enable */
	object->video.enable = (mask & 0x80);

	for (i = 0; i < 7; i ++)
	{
		if (object->audio[i].exists)
		{
			object->audio[i].enable = (mask & (1 << i));
		}
	}

	return 0;

error:
	return -1;
}

char smk_enable_video(smk object, const uint8_t enable)
{
	/* sanity check */
	smk_assert(object);

	object->video.enable = enable;
	return 0;

error:
	return -1;
}

char smk_enable_audio(smk object, const uint8_t track, const uint8_t enable)
{
	/* sanity check */
	smk_assert(object);

	object->audio[track].enable = enable;
	return 0;

error:
	return -1;
}

const uint8_t* smk_get_palette(const smk object)
{
	smk_assert(object);

	return (uint8_t*)object->video.palette;

error:
	return NULL;
}
const uint8_t* smk_get_video(const smk object)
{
	smk_assert(object);

	return object->video.frame;

error:
	return NULL;
}
const uint8_t* smk_get_audio(const smk object, const uint8_t t)
{
	smk_assert(object);

	return object->audio[t].buffer;

error:
	return NULL;
}
uint32_t smk_get_audio_size(const smk object, const uint8_t t)
{
	smk_assert(object);

	return object->audio[t].buffer_size;

error:
	return 0;
}

/* Decompresses a palette-frame. */
static char smk_render_palette(struct smk_video_t* s, uint8_t* p, uint32_t size)
{
	/* Index into palette */
	uint16_t i = 0;
	/* Helper variables */
	uint16_t count, src;

	static uint8_t oldPalette[256][3];

	/* Smacker palette map: smk colors are 6-bit, this table expands them to 8. */
	const uint8_t palmap[64] =
	{
		0x00, 0x04, 0x08, 0x0C, 0x10, 0x14, 0x18, 0x1C,
		0x20, 0x24, 0x28, 0x2C, 0x30, 0x34, 0x38, 0x3C,
		0x41, 0x45, 0x49, 0x4D, 0x51, 0x55, 0x59, 0x5D,
		0x61, 0x65, 0x69, 0x6D, 0x71, 0x75, 0x79, 0x7D,
		0x82, 0x86, 0x8A, 0x8E, 0x92, 0x96, 0x9A, 0x9E,
		0xA2, 0xA6, 0xAA, 0xAE, 0xB2, 0xB6, 0xBA, 0xBE,
		0xC3, 0xC7, 0xCB, 0xCF, 0xD3, 0xD7, 0xDB, 0xDF,
		0xE3, 0xE7, 0xEB, 0xEF, 0xF3, 0xF7, 0xFB, 0xFF
	};

	/* sanity check */
	smk_assert(s);
	smk_assert(p);

	// Copy palette to old palette
	memcpy(oldPalette, s->palette, 256 * 3);

	/* Loop until palette is complete, or we are out of bytes to process */
	while ( (i < 256) && (size > 0) )
	{
		if ((*p) & 0x80)
		{
			/* 0x80: Skip block
				(preserve C+1 palette entries from previous palette) */
			count = ((*p) & 0x7F) + 1;
			p ++; size --;

			/* check for overflow condition */
			if (i + count > 256)
			{
				fprintf(stderr,"libsmacker::palette_render(s,p,size) - ERROR: overflow, 0x80 attempt to skip %d entries from %d\n",count,i);
				goto error;
			}

			/* finally: advance the index. */
			i += count;
		}
		else if ((*p) & 0x40)
		{
			/* 0x40: Color-shift block
				Copy (c + 1) color entries of the previous palette,
				starting from entry (s),
				to the next entries of the new palette. */
			if (size < 2)
			{
				fputs("libsmacker::palette_render(s,p,size) - ERROR: 0x40 ran out of bytes for copy\n",stderr);
				goto error;
			}

			/* pick "count" items to copy */
			count = ((*p) & 0x3F) + 1;
			p ++; size --;

			/* start offset of old palette */
			src = *p;
			p ++; size --;

			/* overflow: see if we write/read beyond 256colors, or overwrite own palette */
			if (i + count > 256 || src + count > 256 ||
				(src < i && src + count > i) )
			{
				fprintf(stderr,"libsmacker::palette_render(s,p,size) - ERROR: overflow, 0x40 attempt to copy %d entries from %d to %d\n",count,src,i);
				goto error;
			}

			/* OK!  Copy the color-palette entries. */
			memmove(&s->palette[i][0],&oldPalette[src][0],count * 3);

			i += count;
		}
		else
		{
			/* 0x00: Set Color block
				Direct-set the next 3 bytes for palette index */
			if (size < 3)
			{
				fprintf(stderr,"libsmacker::palette_render - ERROR: 0x3F ran out of bytes for copy, size=%lu\n", size);
				goto error;
			}

			for (count = 0; count < 3; count ++)
			{
				if (*p > 0x3F)
				{
					fprintf(stderr,"libsmacker::palette_render - ERROR: palette index exceeds 0x3F (entry [%u][%u])\n", i, count);
					goto error;
				}
				s->palette[i][count] = palmap[*p];
				p++; size --;
			}
			i ++;
		}
	}

	if (i < 256)
	{
		fprintf(stderr,"libsmacker::palette_render - ERROR: did not completely fill palette (idx=%u)\n",i);
		goto error;
	}

	return 0;

error:
	/* Error, return -1
		The new palette probably has errors but is preferrable to a black screen */
	return -1;
}

#ifdef TARGET_TWL
#define SMACKER_SWIZZLED
#endif

FAST_FUNC static char smk_render_video(struct smk_video_t* s, uint8_t* p, uint32_t size)
{
	uint8_t* t = s->frame;
	uint8_t s1,s2,s3,s4;
	uint16_t temp;
	uint32_t i,j,k, row, col,skip;

	/* results from a tree lookup */
	int32_t unpack, unpack2, unpack3, unpack4, unpack5;

	/* unpack, broken into pieces */
	uint8_t type;
	uint8_t blocklen;
	uint8_t typedata;
	char bit;

	const uint16_t sizetable[64] = {
		1,	 2,	3,	4,	5,	6,	7,	8,
		9,	10,	11,	12,	13,	14,	15,	16,
		17,	18,	19,	20,	21,	22,	23,	24,
		25,	26,	27,	28,	29,	30,	31,	32,
		33,	34,	35,	36,	37,	38,	39,	40,
		41,	42,	43,	44,	45,	46,	47,	48,
		49,	50,	51,	52,	53,	54,	55,	56,
		57,	58,	59,	128,	256,	512,	1024,	2048
	};

	/* sanity check */
	smk_assert(s);
	smk_assert(p);

	row = 0;
	col = 0;

	/* Set up a bitstream for video unpacking */
	/* We could check the return code but it will only fail if p is null and we already verified that. */
	smk_bs_reset (&s->bs, p, size);

	/* Reset the cache on all bigtrees */
	smk_huff16_reset(s->tree[0]);
	smk_huff16_reset(s->tree[1]);
	smk_huff16_reset(s->tree[2]);
	smk_huff16_reset(s->tree[3]);

	while ( row < s->h )
	{
		smk_huff16_lookup(&s->bs,s->tree[SMK_TREE_TYPE],unpack);

		type = ((unpack & 0x0003));
		blocklen = ((unpack & 0x00FC) >> 2);
		typedata = ((unpack & 0xFF00) >> 8);

		/* support for v4 full-blocks */
		if (type == 1 && s->v == '4')
		{
			smk_bs_read_1(&s->bs, bit);
			if (bit)
			{
				type = 4;
			} else {
				smk_bs_read_1(&s->bs, bit);
				if (bit)
				{
					type = 5;
				}
			}
		}

		for (j = 0; (j < sizetable[blocklen]) && (row < s->h); j ++)
		{
#ifndef SMACKER_SWIZZLED
			skip = (row * s->w) + col;
#else
			skip = (row * s->w) + (col*4);
			//skip = ((row % 4) * 4) + (col % 4) + ((col // 4)*16) + ((row // 4) * pitch * 4);
#endif
			switch(type)
			{
				case 0:
					smk_huff16_lookup(&s->bs,s->tree[SMK_TREE_MCLR],unpack);
					smk_huff16_lookup(&s->bs,s->tree[SMK_TREE_MMAP],unpack2);
					s1 = (unpack & 0xFF00) >> 8;
					s2 = (unpack & 0x00FF);

					temp = 0x01;
					for (k = 0; k < 4; k ++)
					{
						for (i = 0; i < 4; i ++)
						{
							if (unpack2 & temp)
							{
								t[skip + i] = s1;
							}
							else
							{
								t[skip + i] = s2;
							}
							temp = temp << 1;
						}
#ifndef SMACKER_SWIZZLED
						skip += s->w;
#else
						skip += 4;
#endif
					}
					break;

				case 1: /* FULL BLOCK */
#ifndef SMACKER_SWIZZLED
					for (k = 0; k < 4; k ++)
					{
						smk_huff16_lookup(&s->bs,s->tree[SMK_TREE_FULL],unpack);
						smk_huff16_lookup(&s->bs,s->tree[SMK_TREE_FULL],unpack2);
						t[skip] = (unpack2 & 0x00FF);
						t[skip + 1] = ((unpack2 & 0xFF00) >> 8);
						t[skip + 2] = (unpack & 0x00FF);
						t[skip + 3] = ((unpack & 0xFF00) >> 8);
						skip += s->w;
					}
#else
					for (k = 0; k < 2; k ++)
					{
						smk_huff16_lookup(&s->bs,s->tree[SMK_TREE_FULL],unpack);
						smk_huff16_lookup(&s->bs,s->tree[SMK_TREE_FULL],unpack2);
						smk_huff16_lookup(&s->bs,s->tree[SMK_TREE_FULL],unpack3);
						smk_huff16_lookup(&s->bs,s->tree[SMK_TREE_FULL],unpack4);
						*(uint32_t*)&t[skip] = (unpack2 | (unpack << 16));
						*(uint32_t*)&t[skip+4] = (unpack4 | (unpack3 << 16));
						skip += 8;
					}
#endif
					break;
				case 2: /* VOID BLOCK */
					/* break;
					if (s->frame)
					{
						memcpy(&t[skip], &s->frame[skip], 4);
#ifndef SMACKER_SWIZZLED
						skip += s->w;
#else
						skip += 4;
#endif
						memcpy(&t[skip], &s->frame[skip], 4);
#ifndef SMACKER_SWIZZLED
						skip += s->w;
#else
						skip += 4;
#endif
						memcpy(&t[skip], &s->frame[skip], 4);
#ifndef SMACKER_SWIZZLED
						skip += s->w;
#else
						skip += 4;
#endif
						memcpy(&t[skip], &s->frame[skip], 4);
					} */
#ifdef SMACKER_SWIZZLED
					skip += 16;
#endif
					break;
				case 3: /* SOLID BLOCK */
#ifndef SMACKER_SWIZZLED
					memset(&t[skip],typedata,4);
					skip += s->w;
					memset(&t[skip],typedata,4);
					skip += s->w;
					memset(&t[skip],typedata,4);
					skip += s->w;
					memset(&t[skip],typedata,4);
#else
					memset(&t[skip],typedata,4*4);
					skip += 16;
#endif					
					break;
				case 4: /* V4 DOUBLE BLOCK */
#ifndef SMACKER_SWIZZLED
					for (k = 0; k < 2; k ++)
					{
						smk_huff16_lookup(&s->bs,s->tree[SMK_TREE_FULL],unpack);
						s1 = (unpack & 0xFF00) >> 8;
						s2 = (unpack & 0x00FF);
						for (i = 0; i < 2; i ++)
						{
							memset(&t[skip + 2], s1, 2);
							memset(&t[skip], s2, 2);
							skip += s->w;
						}
					}
#else
					smk_huff16_lookup(&s->bs,s->tree[SMK_TREE_FULL],unpack);
					smk_huff16_lookup(&s->bs,s->tree[SMK_TREE_FULL],unpack2);
					s1 = (unpack & 0xFF00) >> 8;
					s2 = (unpack & 0x00FF);
					s3 = (unpack2 & 0xFF00) >> 8;
					s4 = (unpack2 & 0x00FF);
					*(uint32_t*)t[skip] = (s2 | (s2 << 8) | ((s1 | (s1 << 8))<<16));
					*(uint32_t*)t[skip+4] = (s2 | (s2 << 8) | ((s1 | (s1 << 8))<<16));
					*(uint32_t*)t[skip+8] = (s4 | (s4 << 8) | ((s3 | (s3 << 8))<<16));
					*(uint32_t*)t[skip+12] = (s4 | (s4 << 8) | ((s3 | (s3 << 8))<<16));
					skip += 16;
#endif
					break;
				case 5: /* V4 HALF BLOCK */
#ifndef SMACKER_SWIZZLED
					for (k = 0; k < 2; k ++)
					{
						smk_huff16_lookup(&s->bs,s->tree[SMK_TREE_FULL],unpack);
						t[skip + 3] = ((unpack & 0xFF00) >> 8);
						t[skip + 2] = (unpack & 0x00FF);
						t[skip + s->w + 3] = ((unpack & 0xFF00) >> 8);
						t[skip + s->w + 2] = (unpack & 0x00FF);
						smk_huff16_lookup(&s->bs,s->tree[SMK_TREE_FULL],unpack);
						t[skip + 1] = ((unpack & 0xFF00) >> 8);
						t[skip] = (unpack & 0x00FF);
						t[skip + s->w + 1] = ((unpack & 0xFF00) >> 8);
						t[skip + s->w] = (unpack & 0x00FF);
						skip += (s->w << 1);
					}
#else
					smk_huff16_lookup(&s->bs,s->tree[SMK_TREE_FULL],unpack);
					smk_huff16_lookup(&s->bs,s->tree[SMK_TREE_FULL],unpack2);
					smk_huff16_lookup(&s->bs,s->tree[SMK_TREE_FULL],unpack3);
					smk_huff16_lookup(&s->bs,s->tree[SMK_TREE_FULL],unpack4);

					unpack5 = unpack2 | (unpack << 16);
					*(uint32_t*)&t[skip + 0] = unpack5;
					*(uint32_t*)&t[skip + 4] = unpack5;
					unpack5 = unpack4 | (unpack3 << 16);
					*(uint32_t*)&t[skip + 8] = unpack5;
					*(uint32_t*)&t[skip + 12] = unpack5;
					
					skip += 16;
#endif
					break;
			}
			col += 4;
			if (col >= s->w)
			{
				col = 0;
				row += 4;
			}

			if (row > 120) {
				//break;
			}
		}
		if (row > 120) {
			//break;
		}
	}

	return 0;

error:
	return -1;
}

/* Decompress audio track i. */
static char smk_render_audio(struct smk_audio_t* s, uint8_t* p, uint32_t size)
{
	uint32_t j,k;
	uint8_t* t = s->buffer;

	char bit;
	int16_t unpack, unpack2;

	/* used for audio decoding */
	struct smk_huff8_t* aud_tree[4] = {NULL,NULL,NULL,NULL};

	/* sanity check */
	smk_assert(s);
	smk_assert(p);

	if (!s->compress)
	{
		/* Raw PCM data, update buffer size and malloc */
		s->buffer_size = size;

		memcpy(t,p,size);
	}
	else if (s->compress == 1)
	{
		/* SMACKER DPCM compression */
		/* need at least 4 bytes to process */
		if (size < 4)
		{
			fputs("libsmacker::smk_render_audio() - ERROR: need 4 bytes to get unpacked output buffer size.\n",stderr);
			goto error;
		}
		/* chunk is compressed (huff-compressed dpcm), retrieve unpacked buffer size */
		s->buffer_size = ((uint32_t) p[3] << 24) |
						((uint32_t) p[2] << 16) |
						((uint32_t) p[1] << 8) |
						((uint32_t) p[0]);

		p += 4;
		size -= 4;

		/* Compressed audio: must unpack here */
		/*  Set up a bitstream */
		smk_bs_reset (&s->bs, p, size);

		smk_bs_read_1(&s->bs,bit);

		if (!bit)
		{
			fputs("libsmacker::smk_render_audio - ERROR: initial get_bit returned 0\n",stderr);
			goto error;
		}

		smk_bs_read_1(&s->bs,bit);
		if (s->channels != (bit == 1 ? 2 : 1))
		{
			fputs("libsmacker::smk_render - ERROR: mono/stereo mismatch\n",stderr);
		}
		smk_bs_read_1(&s->bs,bit);
		if (s->bitdepth != (bit == 1 ? 16 : 8))
		{
			fputs("libsmacker::smk_render - ERROR: 8-/16-bit mismatch\n",stderr);
		}

		/* build the trees */
		smk_huff8_build(&s->bs,aud_tree[0]);
		j = 1;
		k = 1;
		if (s->bitdepth == 16)
		{
			smk_huff8_build(&s->bs,aud_tree[1]);
			k = 2;
		}
		if (s->channels == 2)
		{
			smk_huff8_build(&s->bs,aud_tree[2]);
			j = 2;
			k = 2;
			if (s->bitdepth == 16)
			{
				smk_huff8_build(&s->bs,aud_tree[3]);
				k = 4;
			}
		}

		/* read initial sound level */
		if (s->channels == 2)
		{
			smk_bs_read_8(&s->bs,unpack);
			if (s->bitdepth == 16)
			{
				smk_bs_read_8(&s->bs,((int16_t*)t)[1])
				((int16_t*)t)[1] |= (unpack << 8);
			}
			else
			{
				((uint8_t*)t)[1] = (uint8_t)unpack;
			}
		}
		smk_bs_read_8(&s->bs,unpack);
		if (s->bitdepth == 16)
		{
				smk_bs_read_8(&s->bs,((int16_t*)t)[0])
				((int16_t*)t)[0] |= (unpack << 8);
		}
		else
		{
			((uint8_t*)t)[0] = (uint8_t)unpack;
		}

		/* All set: let's read some DATA! */
		while (k < s->buffer_size)
		{
			if (s->bitdepth == 8)
			{
				smk_huff8_lookup(&s->bs,aud_tree[0],unpack);
				((uint8_t*)t)[j] = (char)unpack + ((uint8_t*)t)[j - s->channels];
				j ++;
				k++;
			}
			else
			{
				smk_huff8_lookup(&s->bs,aud_tree[0],unpack);
				smk_huff8_lookup(&s->bs,aud_tree[1],unpack2);
				((int16_t*)t)[j] = (int16_t) ( unpack | (unpack2 << 8) ) + ((int16_t*)t)[j - s->channels];
				j ++;
				k+=2;
			}
			if (s->channels == 2)
			{
				if (s->bitdepth == 8)
				{
					smk_huff8_lookup(&s->bs,aud_tree[2],unpack);
					((uint8_t*)t)[j] = (char)unpack + ((uint8_t*)t)[j - 2];
					j ++;
					k++;
				}
				else
				{
					smk_huff8_lookup(&s->bs,aud_tree[2],unpack);
					smk_huff8_lookup(&s->bs,aud_tree[3],unpack2);
					((int16_t*)t)[j] = (int16_t) ( unpack | (unpack2 << 8) ) + ((int16_t*)t)[j - 2];
					j ++;
					k+=2;
				}
			}
		}

		/* All done with the trees, free them. */
		for (j = 0; j < 4; j ++)
		{
			if (aud_tree[j])
			{
				smk_huff8_free(aud_tree[j]);
			}
		}
	}

	return 0;

error:
	/* All done with the trees, free them. */
	for (j = 0; j < 4; j ++)
	{
		if (aud_tree[j])
		{
			smk_huff8_free(aud_tree[j]);
		}
	}

	return -1;
}

/* "Renders" (unpacks) the frame at cur_frame
	Preps all the image and audio pointers */
static char smk_render(smk s)
{
	uint32_t i,size;
	uint8_t* buffer = NULL,* p,track;

	/* sanity check */
	smk_assert(s);

	/* Retrieve current chunk_size for this frame. */
	if (!(i = s->chunk_size[s->cur_frame]))
	{
		fprintf(stderr,"libsmacker::smk_render(s) - Warning: frame %lu: chunk_size is 0.\n",s->cur_frame);
		goto error;
	}

	if (s->mode == SMK_MODE_DISK)
	{
		/* Skip to frame in file */
		if (fseek(s->source.file.fp,s->source.file.chunk_offset[s->cur_frame],SEEK_SET))
		{
			fprintf(stderr,"libsmacker::smk_render(s) - ERROR: fseek to frame %lu (offset %lu) failed.\n",s->cur_frame,s->source.file.chunk_offset[s->cur_frame]);
			perror ("\tError reported was");
			goto error;
		}

		/* In disk-streaming mode: make way for our incoming chunk buffer */
		smk_malloc(buffer, i);

		/* Read into buffer */
		if ( smk_read_file(buffer,s->chunk_size[s->cur_frame],s->source.file.fp) < 0)
		{
			fprintf(stderr,"libsmacker::smk_render(s) - ERROR: frame %lu (offset %lu): smk_read had errors.\n",s->cur_frame,s->source.file.chunk_offset[s->cur_frame]);
			goto error;
		}
	}
	else
	{
		/* Just point buffer at the right place */
		if (!s->source.chunk_data[s->cur_frame])
		{
			fprintf(stderr,"libsmacker::smk_render(s) - ERROR: frame %lu: memory chunk is a NULL pointer.\n",s->cur_frame);
			goto error;
		}
		buffer = s->source.chunk_data[s->cur_frame];
	}

	p = buffer;

	/* Palette record first */
	if (s->frame_type[s->cur_frame] & 0x01)
	{
		/* need at least 1 byte to process */
		if (!i)
		{
			fprintf(stderr,"libsmacker::smk_render(s) - ERROR: frame %lu: insufficient data for a palette rec.\n",s->cur_frame);
			goto error;
		}

		/* Byte 1 in block, times 4, tells how many
			subsequent bytes are present */
		size = 4 * (*p);

		/* If video rendering enabled, kick this off for decode. */
		if (s->video.enable)
		{
			smk_render_palette(&(s->video),p + 1,size - 1);
		}
		p += size;
		i -= size;
	}

	/* Unpack audio chunks */
	for (track = 0; track < 7; track ++)
	{
		if (s->frame_type[s->cur_frame] & (0x02 << track))
		{
			/* need at least 4 byte to process */
			if (i < 4)
			{
				fprintf(stderr,"libsmacker::smk_render(s) - ERROR: frame %lu: insufficient data for audio[%u] rec.\n",s->cur_frame,track);
				goto error;
			}

			/* First 4 bytes in block tell how many
				subsequent bytes are present */
			size = (((uint32_t) p[3] << 24) |
					((uint32_t) p[2] << 16) |
					((uint32_t) p[1] << 8) |
					((uint32_t) p[0]));

			/* If audio rendering enabled, kick this off for decode. */
			if (s->audio[track].enable)
			{
				smk_render_audio(&s->audio[track],p + 4,size - 4);
			}
			p += size;
			i -= size;
		}
		else
		{
			s->audio[track].buffer_size = 0;
		}
	}

	/* Unpack video chunk */
	if (s->video.enable)
	{
		smk_render_video(&(s->video), p,i);
	}

	if (s->mode == SMK_MODE_DISK)
	{
		/* Remember that buffer we allocated?  Trash it */
		smk_free(buffer);
	}

	return 0;

error:
	if (s->mode == SMK_MODE_DISK)
	{
		/* Remember that buffer we allocated?  Trash it */
		smk_free(buffer);
	}

	return -1;
}

/* rewind to first frame and unpack */
char smk_first(smk s)
{
	smk_assert(s);

	s->cur_frame = 0;
	if ( smk_render(s) < 0)
	{
		fprintf(stderr,"libsmacker::smk_first(s) - Warning: frame %lu: smk_render returned errors.\n",s->cur_frame);
		goto error;
	}

	if (s->f == 1) return SMK_LAST;
	return SMK_MORE;

error:
	return -1;
}

/* advance to next frame */
char smk_next(smk s)
{
	smk_assert(s);

	if (s->cur_frame + 1 < (s->f + s->ring_frame))
	{
		s->cur_frame ++;
		if ( smk_render(s) < 0)
		{
			fprintf(stderr,"libsmacker::smk_next(s) - Warning: frame %lu: smk_render returned errors.\n",s->cur_frame);
			goto error;
		}
		if (s->cur_frame + 1 == (s->f + s->ring_frame))
		{
			return SMK_LAST;
		}
		return SMK_MORE;
	}
	else if (s->ring_frame)
	{
		s->cur_frame = 1;
		if ( smk_render(s) < 0)
		{
			fprintf(stderr,"libsmacker::smk_next(s) - Warning: frame %lu: smk_render returned errors.\n",s->cur_frame);
			goto error;
		}
		if (s->cur_frame + 1 == (s->f + s->ring_frame))
		{
			return SMK_LAST;
		}
		return SMK_MORE;
	}
	return SMK_DONE;

error:
	return -1;
}

/* seek to a keyframe in an smk */
char smk_seek_keyframe(smk s, uint32_t f)
{
	smk_assert(s);

	/* rewind (or fast forward!) exactly to f */
	s->cur_frame = f;

	/* roll back to previous keyframe in stream, or 0 if no keyframes exist */
	while (s->cur_frame > 0 && !(s->keyframe[s->cur_frame]))
	{
		s->cur_frame --;
	}

	/* render the frame: we're ready */
	if (smk_render(s) < 0)
	{
		fprintf(stderr,"libsmacker::smk_seek_keyframe(s,%lu) - Warning: frame %lu: smk_render returned errors.\n",f,s->cur_frame);
		goto error;
	}

	return 0;

error:
	return -1;
}

/* seek to a keyframe in an smk */
char smk_seek_keyframe_ahead(smk s, uint32_t f)
{
	smk_assert(s);

	/* rewind (or fast forward!) exactly to f */
	s->cur_frame = f;

	/* roll back to previous keyframe in stream, or 0 if no keyframes exist */
	while (s->cur_frame > 0 && !(s->keyframe[s->cur_frame]))
	{
		s->cur_frame++;
	}

	/* render the frame: we're ready */
	if (smk_render(s) < 0)
	{
		fprintf(stderr,"libsmacker::smk_seek_keyframe(s,%lu) - Warning: frame %lu: smk_render returned errors.\n",f,s->cur_frame);
		goto error;
	}

	return 0;

error:
	return -1;
}

/* check if this frame is a keyframe */
char smk_is_keyframe(smk s)
{
	if (s->cur_frame + 1 < (s->f + s->ring_frame))
	{
		return !!(s->keyframe[s->cur_frame+1]) || (s->chunk_size[s->cur_frame] > 0x6000) || (s->frame_type[s->cur_frame] & 0x01);
	}
	else {
		return 1;
	}
}
