#ifndef __608_H__

#include "ccx_decoders_common.h"
#include "ccx_decoders_xds.h"

extern LLONG ts_start_of_xds;

// Define max width in characters/columns on the screen
#define CCX_DECODER_608_SCREEN_WIDTH  32

/* 
This variable (ccx_decoder_608_report) holds data on the cc channels & xds packets that are encountered during file parse.
This can be interesting if you just want to know what kind of data a file holds that has 608 packets. CCExtractor uses it
for the report functionality.
*/
struct ccx_decoder_608_report_t {
	unsigned xds : 1;
	unsigned cc_channels[4];
} ccx_decoder_608_report; 

typedef struct ccx_decoder_608_settings {	
	int direct_rollup; // Write roll-up captions directly instead of line by line?
	int force_rollup; // 0=Disabled, 1, 2 or 3=max lines in roll-up mode 
	int no_rollup; // If 1, write one line at a time
	unsigned char default_color; // Default color to use.
	int screens_to_process; // How many screenfuls we want? Use -1 for unlimited	
} ccx_decoder_608_settings;

typedef struct ccx_decoder_608_context
{
	ccx_decoder_608_settings settings;
	eia608_screen buffer1;
	eia608_screen buffer2;
	int cursor_row, cursor_column;
	int visible_buffer;
	int screenfuls_counter; // Number of meaningful screenfuls written
	LLONG current_visible_start_ms; // At what time did the current visible buffer became so?
	enum cc_modes mode;
	unsigned char last_c1, last_c2;
	int channel; // Currently selected channel
	unsigned char current_color; // Color we are currently using to write
	unsigned char font; // Font we are currently using to write
	int rollup_base_row;
	LLONG ts_start_of_current_line; /* Time at which the first character for current line was received, =-1 no character received yet */
	LLONG ts_last_char_received; /* Time at which the last written character was received, =-1 no character received yet */
	int new_channel; // The new channel after a channel change		
	int my_field; // Used for sanity checks
	int my_channel; // Used for sanity checks
	long bytes_processed_608; // To be written ONLY by process_608
	struct ccx_s_write *out;
	int have_cursor_position;
	LLONG subs_delay; // ms to delay (or advance) subs
	enum ccx_output_format output_format; // What kind of output format should be used? 
	
	int *halt; // Can be used to halt the feeding of caption data. Set to 1 if screens_to_progress != -1 && screenfuls_counter >= screens_to_process
	int *cc_to_stdout; // If this is set to 1, the stdout will be flushed when data was written to the screen during a process_608 call.
} ccx_decoder_608_context;

extern unsigned char *enc_buffer;
extern unsigned char str[2048]; 
extern unsigned enc_buffer_used;
extern unsigned enc_buffer_capacity;

extern int new_sentence;
extern const char *color_text[][2];

typedef enum ccx_decoder_608_color_code
{
    COL_WHITE = 0,
    COL_GREEN = 1,
    COL_BLUE = 2,
    COL_CYAN = 3,
    COL_RED = 4,
    COL_YELLOW = 5,
    COL_MAGENTA = 6,
    COL_USERDEFINED = 7,
    COL_BLACK = 8,
    COL_TRANSPARENT = 9
} ccx_decoder_608_color_code;


enum font_bits
{
    FONT_REGULAR = 0,
    FONT_ITALICS = 1,
    FONT_UNDERLINED = 2,
    FONT_UNDERLINED_ITALICS = 3
};

enum command_code
{
    COM_UNKNOWN = 0,
    COM_ERASEDISPLAYEDMEMORY = 1,
    COM_RESUMECAPTIONLOADING = 2,
    COM_ENDOFCAPTION = 3,
    COM_TABOFFSET1 = 4,
    COM_TABOFFSET2 = 5,
    COM_TABOFFSET3 = 6,
    COM_ROLLUP2 = 7,
    COM_ROLLUP3 = 8,
    COM_ROLLUP4 = 9,
    COM_CARRIAGERETURN = 10,
    COM_ERASENONDISPLAYEDMEMORY = 11,
    COM_BACKSPACE = 12,
	COM_RESUMETEXTDISPLAY = 13,
	COM_ALARMOFF =14,
	COM_ALARMON = 15,
	COM_DELETETOENDOFROW = 16,
	COM_RESUMEDIRECTCAPTIONING = 17,
	// Non existing commands we insert to have the decoder
	// special stuff for us.
	COM_FAKE_RULLUP1 = 18
};


/*
 * 
 */
ccx_decoder_608_context ccx_decoder_608_init_library(ccx_decoder_608_settings settings, enum ccx_output_format output_format, int channel, int field, LLONG subs_delay, int *halt, int *cc_to_stdout);

/**
 * @param data raw cc608 data to be processed
 *
 * @param length length of data passed
 *
 * @param context context of cc608 where important information related to 608
 * 		  are stored.
 *
 * @param sub pointer to subtitle should be memset to 0 when passed first time
 *            subtitle are stored when structure return
 *
 * @return number of bytes used from data, -1 when any error is encountered
 */
int process608(const unsigned char *data, int length, ccx_decoder_608_context *context, struct cc_subtitle *sub);

/**
 * Issue a EraseDisplayedMemory here so if there's any captions pending
 * they get written to cc_subtitle
 */
void handle_end_of_data(ccx_decoder_608_context *context, struct cc_subtitle *sub);

int write_cc_buffer(ccx_decoder_608_context *context, struct cc_subtitle *sub);
unsigned char *debug_608toASC(unsigned char *ccdata, int channel);

unsigned get_decoder_line_encoded(unsigned char *buffer, int line_num, struct eia608_screen *data);
void capitalize(int line_num, struct eia608_screen *data);
void correct_case(int line_num, struct eia608_screen *data);
void correct_case(int line_num, struct eia608_screen *data);
void capitalize(int line_num, struct eia608_screen *data);
void find_limit_characters(unsigned char *line, int *first_non_blank, int *last_non_blank);
unsigned get_decoder_line_basic(unsigned char *buffer, int line_num, struct eia608_screen *data);
unsigned get_decoder_line_encoded_for_gui(unsigned char *buffer, int line_num, struct eia608_screen *data);
unsigned get_decoder_line_encoded(unsigned char *buffer, int line_num, struct eia608_screen *data);
void delete_all_lines_but_current(ccx_decoder_608_context *context, struct eia608_screen *data, int row);

void get_char_in_latin_1(unsigned char *buffer, unsigned char c);
void get_char_in_unicode(unsigned char *buffer, unsigned char c);
int get_char_in_utf_8(unsigned char *buffer, unsigned char c);
unsigned char cctolower(unsigned char c);
unsigned char cctoupper(unsigned char c);
LLONG get_visible_end(void);

#define __608_H__
#endif