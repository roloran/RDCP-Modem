#ifndef _NSA
#define _NSA

/*
  NSA -- No Strings Attached!
  Basic string operations without Arduino Strings.
*/
#include <Arduino.h> 

#define NSA_MAXSTRLEN 512
#define NSA_MAXPARTS  16

struct nsa_global {
  char result[NSA_MAXSTRLEN];
  char part[NSA_MAXPARTS][NSA_MAXSTRLEN];
};

/**
 * Return the length of a string.
 * @param s String (char*) to analyze
 * @return Length in number of characters (excluding terminating \0)
 */
int nsa_strlen(const char* s);

/**
 * Checks whether the first string starts with the second string.
 * @param s (Longer) string to be checked whether it starts with the other string
 * @param prefix Potential prefix of the other string
 * @return true if `s` starts with `prefix`, false if it does not
 */
bool nsa_startsWith(const char* s, const char* prefix);

/**
 * Checks whether two strings have the same content.
 * @param a First string
 * @param b Seconds string, to be compared to `a`
 * @return true if both strings have the same content, false if they differ somehow
 */
bool nsa_streq(const char* a, const char* b);

/**
 * Extract a substring from a string. The extracted substring is stored as nsa.substring
 * @param s String from which to extract
 * @param begin Index from where to start extraction
 * @param end Index where to stop extraction (i.e., an exclusive end), or -1 for "end of string"
 * @return true if the string was extracted succeeded, false if indices did not fit
 */
bool nsa_substring(const char* s=NULL, int begin=0, int end=-1);

/**
 * Get the index of a substring in a string.
 * @param s The string to analyze for the substring position
 * @param substr The substring to search in `s`
 * @return -1 if `substr` was not found in `s`, or index position if found
 */
int nsa_stridx(const char* s, char* substr);

/**
 * Split a string by given delimiter and store the resulting n-th part in nsa.result
 * @param s String to split
 * @param delim String to split `s` by
 * @param n Which part to extract
 * @return true if the part was a successfully extracted, false requested part was not yielded
 */
bool nsa_strsplit(const char* s=NULL, const char* delim=" ", int n=0);

/**
 * Trim a string and store the result in nsa.result
 * @param s String to trim
 */
void nsa_strtrim(const char* s);

/**
 * Split a string by delimiter and put each i of the parts into nsa.part[i]
 * @param s String to split
 * @param delim Delimiter to split by
 * @return Number of parts created
 */
int nsa_strsplice(const char* s=NULL, const char* delim=" ");

#endif