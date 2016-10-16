/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "../ui_event_source.h"

#include "cocoa.h"

/* --- global functions --- */

int ui_event_source_init(void) { return 1; }

int ui_event_source_final(void) { return 1; }

int ui_event_source_process(void) { return 1; }

int ui_event_source_add_fd(int fd, void (*handler)(void)) {
  cocoa_add_fd(fd, handler);

  return 1;
}

int ui_event_source_remove_fd(int fd) {
  cocoa_remove_fd(fd);

  return 1;
}
