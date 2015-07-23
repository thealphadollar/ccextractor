/* Functions used by both the 608 and 708 decoders. An effort should be
made to reuse, not duplicate, as many functions as possible */

#include "ccx_decoders_common.h"
#include "ccx_common_structs.h"
#include "ccx_common_char_encoding.h"
#include "ccx_common_constants.h"
#include "ccx_common_timing.h"
#include "ccx_common_common.h"
#include "lib_ccx.h"


uint64_t utc_refvalue = UINT64_MAX;  /* _UI64_MAX means don't use UNIX, 0 = use current system time as reference, +1 use a specific reference */
extern int in_xds_mode;

LLONG minimum_fts = 0; // No screen should start before this FTS

/* This function returns a FTS that is guaranteed to be at least 1 ms later than the end of the previous screen. It shouldn't be needed
   obviously but it guarantees there's no timing overlap */
LLONG get_visible_start (void)
{
	LLONG fts = get_fts();
	if (fts <= minimum_fts)
		fts = minimum_fts+1;
	ccx_common_logging.debug_ftn(CCX_DMT_DECODER_608, "Visible Start time=%s\n", print_mstime(fts));
	return fts;
}

/* This function returns the current FTS and saves it so it can be used by get_visible_start */
LLONG get_visible_end (void)
{
	LLONG fts = get_fts();
	if (fts>minimum_fts)
		minimum_fts=fts;
	ccx_common_logging.debug_ftn(CCX_DMT_DECODER_608, "Visible End time=%s\n", print_mstime(fts));
	return fts;
}

int process_cc_data (struct lib_cc_decode *ctx, unsigned char *cc_data, int cc_count, struct cc_subtitle *sub)
{
	int ret = -1;
	for (int j = 0; j < cc_count * 3; j = j + 3)
	{
		if (validate_cc_data_pair( cc_data + j ) )
			continue;
		ret = do_cb(ctx, cc_data + j, sub);
		if (ret == 1) //1 means success here
			ret = 0;
	}
	return ret;
}
int validate_cc_data_pair (unsigned char *cc_data_pair)
{
	unsigned char cc_valid = (*cc_data_pair & 4) >>2;
	unsigned char cc_type = *cc_data_pair & 3;

	if (!cc_valid)
		return -1;

	if (cc_type==0 || cc_type==1)
	{
		// For EIA-608 data we verify parity.
		if (!cc608_parity_table[cc_data_pair[2]])
		{
			// If the second byte doesn't pass parity, ignore pair
			return -1;
		}
		if (!cc608_parity_table[cc_data_pair[1]])
		{
			// The first byte doesn't pass parity, we replace it with a solid blank
			// and process the pair.
			cc_data_pair[1]=0x7F;
		}
	}
	return 0;

}
int do_cb (struct lib_cc_decode *ctx, unsigned char *cc_block, struct cc_subtitle *sub)
{
	unsigned char cc_valid = (*cc_block & 4) >>2;
	unsigned char cc_type = *cc_block & 3;

	int timeok = 1;

	if ( ctx->fix_padding
		&& cc_valid==0 && cc_type <= 1 // Only fix NTSC packets
		&& cc_block[1]==0 && cc_block[2]==0 )
	{
		/* Padding */
		cc_valid=1;
		cc_block[1]=0x80;
		cc_block[2]=0x80;
	}

	if ( ctx->write_format!=CCX_OF_RAW && // In raw we cannot skip padding because timing depends on it
		 ctx->write_format!=CCX_OF_DVDRAW &&
		(cc_block[0]==0xFA || cc_block[0]==0xFC || cc_block[0]==0xFD )
		&& (cc_block[1]&0x7F)==0 && (cc_block[2]&0x7F)==0) // CFS: Skip non-data, makes debugging harder.
		return 1;

	// Print raw data with FTS.
	dbg_print(CCX_DMT_CBRAW, "%s   %d   %02X:%c%c:%02X", print_mstime(fts_now + fts_global),in_xds_mode,
			cc_block[0], cc_block[1]&0x7f,cc_block[2]&0x7f, cc_block[2]);

	/* In theory the writercwtdata() function could return early and not
	 * go through the 608/708 cases below.  We do that to get accurate
	 * counts for cb_field1, cb_field2 and cb_708.
	 * Note that printdata() and do_708() must not be called for
	 * the CCX_OF_RCWT case. */

	if (cc_valid || cc_type==3)
	{
		ctx->cc_stats[cc_type]++;

		switch (cc_type)
		{
			case 0:
				dbg_print(CCX_DMT_CBRAW, "    %s   ..   ..\n",  debug_608toASC( cc_block, 0));

				current_field=1;
				ctx->saw_caption_block = 1;

				if (ctx->extraction_start.set &&
						get_fts() < ctx->extraction_start.time_in_ms)
					timeok = 0;
				if (ctx->extraction_end.set &&
						get_fts() > ctx->extraction_end.time_in_ms)
				{
					timeok = 0;
					ctx->processed_enough=1;
				}
				if (timeok)
				{
					if(ctx->write_format!=CCX_OF_RCWT)
						printdata (ctx, cc_block+1,2,0,0, sub);
					else
						writercwtdata(ctx, cc_block, sub);
				}
				cb_field1++;
				break;
			case 1:
				dbg_print(CCX_DMT_CBRAW, "    ..   %s   ..\n",  debug_608toASC( cc_block, 1));

				current_field=2;
				ctx->saw_caption_block = 1;

				if (ctx->extraction_start.set &&
						get_fts() < ctx->extraction_start.time_in_ms)
					timeok = 0;
				if (ctx->extraction_end.set &&
						get_fts() > ctx->extraction_end.time_in_ms)
				{
					timeok = 0;
					ctx->processed_enough=1;
				}
				if (timeok)
				{
					if(ctx->write_format!=CCX_OF_RCWT)
						printdata (ctx, 0,0,cc_block+1,2, sub);
					else
						writercwtdata(ctx, cc_block, sub);
				}
				cb_field2++;
				break;
			case 2: //EIA-708
				// DTVCC packet data
				// Fall through
			case 3: //EIA-708
				dbg_print(CCX_DMT_CBRAW, "    ..   ..   DD\n");

				// DTVCC packet start
				current_field=3;

				if (ctx->extraction_start.set &&
						get_fts() < ctx->extraction_start.time_in_ms)
					timeok = 0;
				if (ctx->extraction_end.set &&
						get_fts() > ctx->extraction_end.time_in_ms)
				{
					timeok = 0;
					ctx->processed_enough=1;
				}
				char temp[4];
				temp[0]=cc_valid;
				temp[1]=cc_type;
				temp[2]=cc_block[1];
				temp[3]=cc_block[2];
				if (timeok)
				{
					if(ctx->write_format!=CCX_OF_RCWT)
						do_708 (ctx,(const unsigned char *) temp, 4);
					else
						writercwtdata(ctx, cc_block, sub);
				}
				cb_708++;
				// Check for bytes read
				// printf ("Warning: Losing EIA-708 data!\n");
				break;
			default:
				fatal(CCX_COMMON_EXIT_BUG_BUG, "Cannot be reached!");
		} // switch (cc_type)
	} // cc_valid
	else
	{
		dbg_print(CCX_DMT_CBRAW, "    ..   ..   ..\n");
		dbg_print(CCX_DMT_VERBOSE, "Found !(cc_valid || cc_type==3) - ignore this block\n");
	}

	return 1;
}
struct lib_cc_decode* init_cc_decode (struct ccx_decoders_common_settings_t *setting)
{
	struct lib_cc_decode *ctx = NULL;

	ctx = malloc(sizeof(struct lib_cc_decode));
	if(!ctx)
		return NULL;

	ctx->avc_ctx = init_avc();

	if(setting->codec == CCX_CODEC_ATSC_CC)
	{
		// Prepare 608 context
		ctx->context_cc608_field_1 = ccx_decoder_608_init_library(
				setting->settings_608,
				setting->cc_channel,
				1,
				&ctx->processed_enough,
				setting->cc_to_stdout,
				setting->output_format
				);
		ctx->context_cc608_field_2 = ccx_decoder_608_init_library(
				setting->settings_608,
				setting->cc_channel,
				2,
				&ctx->processed_enough,
				setting->cc_to_stdout,
				setting->output_format
				);
	}
	ctx->private_data = setting->private_data;
	ctx->fix_padding = setting->fix_padding;
	ctx->write_format =  setting->output_format;
	ctx->subs_delay =  setting->subs_delay;
	ctx->extract = setting->extract;
	ctx->fullbin = setting->fullbin;
	ctx->hauppauge_mode = setting->hauppauge_mode;
	ctx->program_number = setting->program_number;
	ctx->processed_enough = 0;
	ctx->timing = init_timing_ctx(&ccx_common_timing_settings);
	memcpy(&ctx->extraction_start, &setting->extraction_start,sizeof(struct ccx_boundary_time));
	memcpy(&ctx->extraction_end, &setting->extraction_end,sizeof(struct ccx_boundary_time));

	if (setting->send_to_srv)
		ctx->writedata = net_send_cc;
	else if (setting->output_format==CCX_OF_RAW ||
		setting->output_format==CCX_OF_DVDRAW ||
		setting->output_format==CCX_OF_RCWT )
		ctx->writedata = writeraw;
	else if (setting->output_format==CCX_OF_SMPTETT ||
		setting->output_format==CCX_OF_SAMI ||
		setting->output_format==CCX_OF_SRT ||
		setting->output_format==CCX_OF_TRANSCRIPT ||
		setting->output_format==CCX_OF_SPUPNG ||
		setting->output_format==CCX_OF_NULL)
		ctx->writedata = process608;
	else
		fatal(CCX_COMMON_EXIT_BUG_BUG, "Invalid Write Format Selected");

	memset (&ctx->dec_sub, 0,sizeof(ctx->dec_sub));

	return ctx;
}

void flush_cc_decode(struct lib_cc_decode *ctx, struct cc_subtitle *sub)
{
	if (ctx->extract != 2)
	{
		if (ctx->write_format==CCX_OF_SMPTETT || ctx->write_format==CCX_OF_SAMI || 
			ctx->write_format==CCX_OF_SRT || ctx->write_format==CCX_OF_TRANSCRIPT
			|| ctx->write_format==CCX_OF_SPUPNG )
		{
			flush_608_context(ctx->context_cc608_field_1, sub);
		}
		else if(ctx->write_format == CCX_OF_RCWT)
		{
			// Write last header and data
			writercwtdata (ctx, NULL, sub);
		}
	}
	if (ctx->extract != 1)
	{
		if (ctx->write_format == CCX_OF_SMPTETT || ctx->write_format == CCX_OF_SAMI ||
			ctx->write_format == CCX_OF_SRT || ctx->write_format == CCX_OF_TRANSCRIPT
			|| ctx->write_format == CCX_OF_SPUPNG )
		{
			flush_608_context(ctx->context_cc608_field_2, sub);
		}
	}

}
void dinit_cc_decode(struct lib_cc_decode **ctx)
{
	struct lib_cc_decode *lctx = *ctx;
	dinit_avc(&lctx->avc_ctx);
	ccx_decoder_608_dinit_library(&lctx->context_cc608_field_1);
	ccx_decoder_608_dinit_library(&lctx->context_cc608_field_2);
	freep(ctx);
}
