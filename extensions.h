
#ifndef _FILE_EXTENSIONS_
#define _FILE_EXTENSIONS_

#define VIDEO_EXT 1
#define PHOTO_EXT 2
#define AUDIO_EXT 3

struct _ext
{
	const char* ext;
	int type;
};

struct _ext extensions[] = 
	{
		{ "3gp",  VIDEO_EXT },
		{ "aac",  AUDIO_EXT },
		{ "asf",  VIDEO_EXT },
		{ "asx",  VIDEO_EXT },
		{ "avi",  VIDEO_EXT },
		{ "dv",   VIDEO_EXT },
		{ "fli",  VIDEO_EXT },
		{ "jiff", PHOTO_EXT },
		{ "jpe",  PHOTO_EXT },
		{ "jpeg", PHOTO_EXT },
		{ "jpg",  PHOTO_EXT },
		{ "lsf",  VIDEO_EXT },
		{ "mkv",  VIDEO_EXT },
		{ "mng",  VIDEO_EXT },
		{ "mov",  VIDEO_EXT },
		{ "movie", VIDEO_EXT },
		{ "mp3",  AUDIO_EXT },
		{ "mp4",  VIDEO_EXT },
		{ "mpe",  VIDEO_EXT },
		{ "mpeg", VIDEO_EXT },
		{ "mpg",  VIDEO_EXT },
		{ "ogg",  AUDIO_EXT },
		{ "png",  PHOTO_EXT },
		{ "qt",   VIDEO_EXT },
		{ "tif",  PHOTO_EXT },
		{ "tiff", PHOTO_EXT },
		{ "vob",  VIDEO_EXT },
		{ "was",  AUDIO_EXT },
		{ "wm",   VIDEO_EXT },
		{ "wmv",  VIDEO_EXT },
		{ "wmx",  VIDEO_EXT },
		{ "wvx",  VIDEO_EXT },
	};

int extensions_size = sizeof(extensions) / sizeof(struct _ext);

#endif
