#ifndef GBLUE_H_
#define GBLUE_H_
#include <linux/ioctl.h>

#define GBLUE_IOCTRL_MAGIC  'a'

#define GBLUE_IOCTRL_ENABLE				_IOW(GBLUE_IOCTRL_MAGIC, 0, int)
#define GBLUE_IOCTRL_NEXT				_IO(GBLUE_IOCTRL_MAGIC, 1)
#define GBLUE_IOCTRL_PLAY_PAUSE			_IO(GBLUE_IOCTRL_MAGIC, 2)
#define GBLUE_IOCTRL_PREV				_IO(GBLUE_IOCTRL_MAGIC, 3)
#define GBLUE_IOCTRL_CLEAR_PAIR			_IO(GBLUE_IOCTRL_MAGIC, 4)

#define GBLUE_IOCTRL_MAXNR 4

enum{
	STATUS_DISABLED		= 0,
	STATUS_ENABLED		= 1,
	STATUS_DISCONNECTED	= 2,
	STATUS_CONNECTED	= 3,
};

#endif
