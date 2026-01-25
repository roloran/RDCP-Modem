#include <Arduino.h>
#include "nsa.h"

nsa_global nsa;

int nsa_strlen(const char* s)
{
  int len = 0;
  if (s == NULL) return 0;
  while (s[len] != '\0') len++;
  return len;
}

bool nsa_startsWith(const char* s, const char* prefix)
{
  int i = 0;
  if (s == NULL || prefix == NULL) return false;

  while (prefix[i] != '\0') 
  {
    if (s[i] == '\0') return false;
    if (s[i] != prefix[i]) return false;
    i++;
  }
  return true;
}

bool nsa_streq(const char* a, const char* b)
{
  int i = 0;
  if (a == NULL || b == NULL) return false;

  while (a[i] != '\0' && b[i] != '\0') 
  {
    if (a[i] != b[i]) return false;
    i++;
  }
  if (a[i] != '\0' || b[i] != '\0') return false;
  return true;
}

bool nsa_substring(const char* s, int begin, int end)
{
  int i;
  int len = 0;
  int finito = end - 1;

  if (s == NULL || begin < 0) return false;
  while (s[len] != '\0') len++;
  if (end == -1) finito = len - 1;
  if (finito < begin || finito >= len) return false;

  for (i = begin; i <= finito; i++) nsa.result[i - begin] = s[i];
  nsa.result[finito - begin + 1] = '\0';
  return true;
}

int nsa_stridx(const char* s, const char* substr)
{
  int i, j;
  if (s == NULL || substr == NULL) return -1;
  if (substr[0] == '\0') return 0;

  for (i = 0; s[i] != '\0'; i++) 
  {
    j = 0;
    while (s[i+j] != '\0' && substr[j] != '\0' && s[i+j] == substr[j]) j++;
    if (substr[j] == '\0') return i;
  }
  return -1;
}

bool nsa_strsplit(const char* s, const char* delim, int n)
{
  int i = 0;
  int part = 0;
  int start = 0;
  int j;
  int delim_len = 0;

  if (s == NULL || delim == NULL || n < 0) return false;

  while (delim[delim_len] != '\0') delim_len++;
  if (delim_len == 0) return false;

  while (s[i] != '\0') 
  {
    j = 0;
    while (delim[j] != '\0' && s[i + j] == delim[j]) j++;
    if (delim[j] == '\0') 
    {
      if (part == n) 
      {
        int len = i - start;
        for (j = 0; j < len; j++) nsa.result[j] = s[start + j];
        nsa.result[len] = '\0';
        return true;
      }
      part++;
      i += delim_len;
      start = i;
    } 
    else i++;
  }

  if (part == n) 
  {
    int len = i - start;
    for (j = 0; j < len; j++) nsa.result[j] = s[start + j];
    nsa.result[len] = '\0';
    return true;
  }

  return false;
}

void nsa_strtrim(const char* s)
{
  int start = 0;
  int end, i;
  if (s == NULL) 
  {
    nsa.result[0] = '\0';
    return;
  }
  while (s[start] == ' '  || s[start] == '\t' || s[start] == '\n' || s[start] == '\r') start++;
  end = start;
  while (s[end] != '\0') end++;
  end--;
  while (end >= start && (s[end] == ' ' || s[end] == '\t' || s[end] == '\n' || s[end] == '\r')) end--;
  for (i = start; i <= end; i++) nsa.result[i - start] = s[i];
  nsa.result[end - start + 1] = '\0';
  return;
}

int nsa_strsplice(const char* s, const char* delim)
{
  int i = 0;
  int j;
  int start = 0;
  int count = 0;
  int delim_len = 0;

  if (s == NULL || delim == NULL) return 0;
  while (delim[delim_len] != '\0') delim_len++;
  if (delim_len == 0) return 0;

  i = 0;
  count = 0;
  while (s[i] != '\0') 
  {
    j = 0;
    while (delim[j] != '\0' && s[i + j] == delim[j]) j++;
    if (delim[j] == '\0') 
    {
      int len = i - start;
      int k;
      for (k = 0; k < len; k++) nsa.part[count][k] = s[start + k];
      nsa.part[count][len] = '\0';
      count++;
      i += delim_len;
      start = i;
    } 
    else i++;
  }

  int len = i - start;
  int k;
  for (k = 0; k < len; k++) nsa.part[count][k] = s[start + k];
  nsa.part[count][len] = '\0';

  return count;
}
