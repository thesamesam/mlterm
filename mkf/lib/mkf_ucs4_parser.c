/*
 *	update: <2001/11/26(02:45:27)>
 *	$Id$
 */

#include  "mkf_ucs4_parser.h"

#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>

#include  "mkf_ucs_property.h"


typedef struct  mkf_ucs4_parser
{
	mkf_parser_t  parser ;
	int  is_big_endian ;

} mkf_ucs4_parser_t ;



/* --- static functions --- */

static void
ucs4_parser_init(
	mkf_parser_t *  parser
	)
{
	mkf_ucs4_parser_t *  ucs4_parser ;

	mkf_parser_init( parser) ;

	ucs4_parser = (mkf_ucs4_parser_t*) parser ;
	
	ucs4_parser->is_big_endian = 1 ;
}

static void
ucs4_parser_set_str(
	mkf_parser_t *  ucs4_parser ,
	u_char *  str ,
	size_t  size
	)
{
	ucs4_parser->str = str ;
	ucs4_parser->left = size ;
	ucs4_parser->marked_left = 0 ;
	ucs4_parser->is_eos = 0 ;
}

static void
ucs4_parser_delete(
	mkf_parser_t *  s
	)
{
	free( s) ;
}

static int
ucs4_parser_next_char(
	mkf_parser_t *  ucs4_parser ,
	mkf_char_t *  ucs4_ch
	)
{
	mkf_ucs4_parser_t *  __ucs4_parser ;

	__ucs4_parser = (mkf_ucs4_parser_t*)ucs4_parser ;
	
	if( ucs4_parser->left < 4)
	{
		ucs4_parser->is_eos = 1 ;
	
		return  0 ;
	}

	if( memcmp( ucs4_parser->str , "\x00\x00\xfe\xff" , 4) == 0)
	{
		__ucs4_parser->is_big_endian = 1 ;

		mkf_parser_n_increment( ucs4_parser , 4) ;

		return  ucs4_parser_next_char( ucs4_parser , ucs4_ch) ;
	}
	else if( memcmp( ucs4_parser->str , "\xff\xfe\x00\x00" , 4) == 0)
	{
		__ucs4_parser->is_big_endian = 0 ;
		
		mkf_parser_n_increment( ucs4_parser , 4) ;

		return  ucs4_parser_next_char( ucs4_parser , ucs4_ch) ;
	}
	else
	{
		mkf_ucs_property_t  prop ;
		
		if( __ucs4_parser->is_big_endian)
		{
			memcpy( ucs4_ch->ch , ucs4_parser->str , 4) ;
		}
		else
		{
			ucs4_ch->ch[0] = ucs4_parser->str[3] ;
			ucs4_ch->ch[1] = ucs4_parser->str[2] ;
			ucs4_ch->ch[2] = ucs4_parser->str[1] ;
			ucs4_ch->ch[3] = ucs4_parser->str[0] ;
		}

		mkf_parser_n_increment( ucs4_parser , 4) ;

		ucs4_ch->cs = ISO10646_UCS4_1 ;
		ucs4_ch->size = 4 ;

		prop = mkf_get_ucs_property( ucs4_ch->ch , ucs4_ch->size) ;
		if( UCSPROP_GENERAL_CATEGORY(prop) == MKF_UCSPROP_MC ||
			UCSPROP_GENERAL_CATEGORY(prop) == MKF_UCSPROP_ME ||
			UCSPROP_GENERAL_CATEGORY(prop) == MKF_UCSPROP_MN)
		{
			ucs4_ch->property = MKF_COMBINING ;
		}
		else
		{
			ucs4_ch->property = 0 ;
		}

		return  1 ;
	}
}


/* --- global functions --- */

mkf_parser_t *
mkf_ucs4_parser_new(void)
{
	mkf_ucs4_parser_t *  ucs4_parser ;

	if( ( ucs4_parser = malloc( sizeof( mkf_ucs4_parser_t))) == 0)
	{
		return  NULL ;
	}
	
	ucs4_parser_init( (mkf_parser_t*) ucs4_parser) ;

	ucs4_parser->parser.init = ucs4_parser_init ;
	ucs4_parser->parser.set_str = ucs4_parser_set_str ;
	ucs4_parser->parser.delete = ucs4_parser_delete ;
	ucs4_parser->parser.next_char = ucs4_parser_next_char ;

	return  (mkf_parser_t*) ucs4_parser ;
}
