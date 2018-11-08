#ifndef REGEX_H
#define REGEX_H
// Based on simple regex implementation by Rob Pike
// Supports
// - Matching literal characters
// - Matching any character with .
// - Matching multiple characters with * or +
// - ? for optional characters
// - ^ & $ for positional fixing

/* regex: search for regexp anywhere in text */
int regexp(char *regexp, char *text);
#endif
