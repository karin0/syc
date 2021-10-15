#pragma once

#include <ostream>

// #define SYC_SYNTAX_PROMPT
// #define SYC_ERROR_PROMPT

void set_os(std::ostream &outs);
std::ostream &get_os();

#ifdef SYC_ERROR_PROMPT

void push_err(int ty, int ln);
bool put_errs();

void push_err_mask();
void pop_err_mask_resolve();
void pop_err_mask_reject();

#define HANDLE_ERR(whatever) whatever

#else

#define HANDLE_ERR(whatever)

#endif
