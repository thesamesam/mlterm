/*
 *	$Id$
 */

#include  "x_event_source.h"

#include  <kiklib/kik_def.h>	/* USE_WIN32API */

#ifndef  USE_WIN32API
#include  <string.h>		/* memset/memcpy */
#include  <sys/time.h>		/* timeval */
#include  <unistd.h>		/* select */
#include  <kiklib/kik_file.h>	/* kik_file_set_cloexec */
#endif

#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>	/* alloca/kik_alloca_garbage_collect/malloc/free */
#include  <kiklib/kik_types.h>	/* u_int */
#include  <ml_term_manager.h>

#include  "x_display.h"
#include  "x_screen_manager.h"


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

#ifndef  USE_WIN32API
static struct
{
	int  fd ;
	void (*handler)( void) ;

} * additional_fds ;
static u_int  num_of_additional_fds ;
#endif


/* --- static functions --- */

#ifdef  USE_WIN32API

static VOID CALLBACK
timer_proc(
	HWND  hwnd,
	UINT  msg,
	UINT  timerid,
	DWORD  time
	)
{
	x_display_t **  displays ;
	u_int  num_of_displays ;
	int  count ;
	
	displays = x_get_opened_displays( &num_of_displays) ;

	for( count = 0 ; count < num_of_displays ; count ++)
	{
		x_display_idling( displays[count]) ;
	}
}

#else	/* USE_WIN32API */

static void
receive_next_event(void)
{
	u_int  count ;
	ml_term_t **  terms ;
	u_int  num_of_terms ;
	int  xfd ;
	int  ptyfd ;
#ifdef  USE_LIBSSH2
	int *  xssh_fds ;
	u_int  num_of_xssh_fds ;
#endif
	int  maxfd ;
	int  ret ;
	fd_set  read_fds ;
	struct timeval  tval ;
	x_display_t **  displays ;
	u_int  num_of_displays ;

	num_of_terms = ml_get_all_terms( &terms) ;

	while( 1)
	{
		/* on Linux tv_usec,tv_sec members are zero cleared after select() */
	#if  defined(__NetBSD__) && defined(USE_FRAMEBUFFER)
		static int  display_idling_wait = 4 ;

		tval.tv_usec = 25000 ;	/* 0.025 sec */
	#else
		tval.tv_usec = 100000 ;	/* 0.1 sec */
	#endif
		tval.tv_sec = 0 ;

		maxfd = 0 ;
		FD_ZERO( &read_fds) ;

		displays = x_get_opened_displays( &num_of_displays) ;
		
		for( count = 0 ; count < num_of_displays ; count ++)
		{
			x_display_receive_next_event( displays[count]) ;
			
			xfd = x_display_fd( displays[count]) ;
			
			FD_SET( xfd , &read_fds) ;
		
			if( xfd > maxfd)
			{
				maxfd = xfd ;
			}
		}

		for( count = 0 ; count < num_of_terms ; count ++)
		{
			ptyfd = ml_term_get_master_fd( terms[count]) ;
			FD_SET( ptyfd , &read_fds) ;

			if( ptyfd > maxfd)
			{
				maxfd = ptyfd ;
			}
		}
		
	#ifdef  USE_LIBSSH2
		num_of_xssh_fds = ml_pty_ssh_get_x11_fds( &xssh_fds) ;

		for( count = 0 ; count < num_of_xssh_fds ; count++)
		{
			FD_SET( xssh_fds[count] , &read_fds) ;
			if( xssh_fds[count] > maxfd)
			{
				maxfd = xssh_fds[count] ;
			}
		}
	#endif

		for( count = 0 ; count < num_of_additional_fds ; count++)
		{
			if( additional_fds[count].fd >= 0)
			{
				FD_SET( additional_fds[count].fd , &read_fds) ;
				
				if( additional_fds[count].fd > maxfd)
				{
					maxfd = additional_fds[count].fd ;
				}
			}
		}

		if( ( ret = select( maxfd + 1 , &read_fds , NULL , NULL , &tval)) != 0)
		{
			break ;
		}

		for( count = 0 ; count < num_of_additional_fds ; count++)
		{
			if( additional_fds[count].fd < 0)
			{
				(*additional_fds[count].handler)() ;
			}
		}

	#if  defined(__NetBSD__) && defined(USE_FRAMEBUFFER)
		/* x_display_idling() is called every 0.1 sec. */
		if( -- display_idling_wait > 0)
		{
			continue ;
		}
		display_idling_wait = 4 ;
	#endif

		for( count = 0 ; count < num_of_displays ; count ++)
		{
			x_display_idling( displays[count]) ;
		}
	}
	
	if( ret < 0)
	{
		/* error happened */
		
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " error happened in select. ") ;
		perror( NULL) ;
	#endif

		return ;
	}

	/*
	 * Processing order should be as follows.
	 *
	 * X Window -> PTY -> additional_fds
	 *
	 * (ml_pty_ssh_send_recv_x11() should be called after ml_term_parse_vt100_sequence()
	 * which can process x11 forwarding channel packets.)
	 */

	for( count = 0 ; count < num_of_displays ; count ++)
	{
		if( FD_ISSET( x_display_fd( displays[count]) , &read_fds))
		{
			x_display_receive_next_event( displays[count]) ;
		}
	}

	for( count = 0 ; count < num_of_terms ; count ++)
	{
		if( FD_ISSET( ml_term_get_master_fd( terms[count]) , &read_fds))
		{
			ml_term_parse_vt100_sequence( terms[count]) ;
		}
	}

#ifdef  USE_LIBSSH2
	for( count = num_of_xssh_fds ; count > 0 ; count--)
	{
		ml_pty_ssh_send_recv_x11( count - 1 ,
			FD_ISSET( xssh_fds[count - 1] , &read_fds)) ;
	}
#endif

	for( count = 0 ; count < num_of_additional_fds ; count++)
	{
		if( additional_fds[count].fd >= 0)
		{
			if( FD_ISSET( additional_fds[count].fd , &read_fds))
			{
				(*additional_fds[count].handler)() ;

				break ;
			}
		}
	}

	return ;
}

#endif


/* --- global functions --- */

int
x_event_source_init(void)
{
#ifdef  USE_WIN32API
	/* x_window_manager_idling() called in 0.1sec. */
	SetTimer( NULL, 0, 100, timer_proc) ;
#endif

	return  1 ;
}

int
x_event_source_final(void)
{
#if  ! defined(USE_WIN32GUI) && ! defined(DEBUG)
	exit(0) ;
#endif

#ifndef  USE_WIN32API
	free( additional_fds) ;
#endif

	return  1 ;
}

int
x_event_source_process(void)
{
#ifdef  USE_WIN32API
	u_int  num_of_displays ;
	x_display_t **  displays ;
	ml_term_t **  terms ;
	u_int  num_of_terms ;
	int *  xssh_fds ;
	u_int  count ;
#endif

#ifdef  USE_WIN32API
	displays = x_get_opened_displays( &num_of_displays) ;
	for( count = 0 ; count < num_of_displays ; count++)
	{
		x_display_receive_next_event( displays[count]) ;
	}
#else
	receive_next_event() ;
#endif

	ml_close_dead_terms() ;

#ifdef  USE_WIN32API
	/*
	 * XXX
	 * If pty is closed after ml_close_dead_terms() ...
	 */

#ifdef  USE_LIBSSH2
	for( count = ml_pty_ssh_get_x11_fds( &xssh_fds) ; count > 0 ; count--)
	{
		ml_pty_ssh_send_recv_x11( count - 1 , 1) ;
	}
#endif

	num_of_terms = ml_get_all_terms( &terms) ;

	for( count = 0 ; count < num_of_terms ; count++)
	{
		ml_term_parse_vt100_sequence( terms[count]) ;
	}
#endif

	x_close_dead_screens() ;

	if( x_get_all_screens( NULL) == 0)
	{
		return  0 ;
	}

	return  1 ;
}

/*
 * fd >= 0  -> Normal file descriptor. handler is invoked if fd is ready.
 * fd < 0 -> Special ID. handler is invoked at interval of 0.1 sec.
 */
int
x_event_source_add_fd(
	int  fd ,
	void  (*handler)(void)
	)
{
#ifndef  USE_WIN32API

	void *  p ;

	if( ! handler)
	{
		return  0 ;
	}

	if( ( p = realloc( additional_fds ,
			sizeof(*additional_fds) * (num_of_additional_fds + 1))) == NULL)
	{
		return  0 ;
	}

	additional_fds = p ;
	additional_fds[num_of_additional_fds].fd = fd ;
	additional_fds[num_of_additional_fds++].handler = handler ;
	if( fd >= 0)
	{
		kik_file_set_cloexec( fd) ;
	}

#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %d is added to additional fds.\n" , fd) ;
#endif

	return  1 ;

#else	/* USE_WIN32API */

	return  0 ;

#endif	/* USE_WIN32API */
}

int
x_event_source_remove_fd(
	int  fd
	)
{
#ifndef  USE_WIN32API
	u_int  count ;

	for( count = 0 ; count < num_of_additional_fds ; count++)
	{
		if( additional_fds[count].fd == fd)
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " Additional fd %d is removed.\n" , fd) ;
		#endif
		
			additional_fds[count] = additional_fds[--num_of_additional_fds] ;

			return  1 ;
		}
	}
#endif	/* USE_WIN32API */

	return  0 ;
}

