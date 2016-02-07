#ifndef __VDISPLAY_H__
#define __VDISPLAY_H__

#define INDEX_REG 	0x3d4	/* For selecting a regster[0-11h] */
#define DATA_REG	0x3d5	/* Selected register		  */
#define MODE_REG	0x3d8	
#define COLOR_REG	0x3d9	/* Color selection in text mode	  */
#define STATUS_REG	0x3da
#define CURSOR_START_REGNO	0x0a
#define CURSOR_END_REGNO	0x0b
#define START_ADDR_MSB_REGNO	0x0c
#define START_ADDR_LSB_REGNO	0x0d
#define CURSOR_ADDR_MSB_REGNO	0x0e
#define CURSOR_ADDR_LSB_REGNO	0x0f

#define MODE_80x25_TEXT	0x42

/* 
 * Video device parameters (Not yet complete).
 */
struct video_param {
	int cursor_x,cursor_y,attrib;
};

void video_init(void);
void output_proc(char *text, int len);

void scroll_up(int lines);
void scroll_down(int lines);
struct video_param get_video_param(void);
int set_video_param(struct video_param * vp);

#endif
