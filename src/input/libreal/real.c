/*
 * Copyright (C) 2002 the xine project
 *
 * This file is part of xine, a free video player.
 *
 * xine is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * xine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 * $Id: real.c,v 1.2 2002/12/14 00:02:31 holstsn Exp $
 *
 * special functions for real streams.
 * adopted from joschkas real tools.
 *
 */

#include <stdio.h>
#include <string.h>

#include "real.h"
#include "asmrp.h"

/*
#define LOG
*/

const unsigned char xor_table[] = {
    0x05, 0x18, 0x74, 0xd0, 0x0d, 0x09, 0x02, 0x53,
    0xc0, 0x01, 0x05, 0x05, 0x67, 0x03, 0x19, 0x70,
    0x08, 0x27, 0x66, 0x10, 0x10, 0x72, 0x08, 0x09,
    0x63, 0x11, 0x03, 0x71, 0x08, 0x08, 0x70, 0x02,
    0x10, 0x57, 0x05, 0x18, 0x54, 0x00, 0x00, 0x00 };


#define BE_32C(x,y) x[3]=(char)(y & 0xff);\
    x[2]=(char)((y >> 8) & 0xff);\
    x[1]=(char)((y >> 16) & 0xff);\
    x[0]=(char)((y >> 24) & 0xff);

#define BE_16(x)  ((((uint8_t*)(x))[0] << 8) | ((uint8_t*)(x))[1])

#define BE_32(x)  ((((uint8_t*)(x))[0] << 24) | \
                   (((uint8_t*)(x))[1] << 16) | \
                   (((uint8_t*)(x))[2] << 8) | \
                    ((uint8_t*)(x))[3])

#define MAX(x,y) ((x>y) ? x : y)

#ifdef LOG
static void hexdump (const char *buf, int length) {

  int i;

  printf (" hexdump> ");
  for (i = 0; i < length; i++) {
    unsigned char c = buf[i];

    printf ("%02x", c);

    if ((i % 16) == 15)
      printf ("\n         ");

    if ((i % 2) == 1)
      printf (" ");

  }
  printf ("\n");
}
#endif


static void hash(char *field, char *param) {

  uint32_t a, b, c, d;
 

  /* fill variables */
  memcpy(&a, field, sizeof(uint32_t));
  memcpy(&b, &field[4], sizeof(uint32_t));
  memcpy(&c, &field[8], sizeof(uint32_t));
  memcpy(&d, &field[12], sizeof(uint32_t));

#ifdef LOG
  printf("real: hash input: %x %x %x %x\n", a, b, c, d);
  printf("real: hash parameter:\n");
  hexdump(param, 64);
#endif
  
  a = ((b & c) | (~b & d)) + *((uint32_t*)(param+0x00)) + a - 0x28955B88;
  a = ((a << 0x07) | (a >> 0x19)) + b;
  d = ((a & b) | (~a & c)) + *((uint32_t*)(param+0x04)) + d - 0x173848AA;
  d = ((d << 0x0c) | (d >> 0x14)) + a;
  c = ((d & a) | (~d & b)) + *((uint32_t*)(param+0x08)) + c + 0x242070DB;
  c = ((c << 0x11) | (c >> 0x0f)) + d;
  b = ((c & d) | (~c & a)) + *((uint32_t*)(param+0x0c)) + b - 0x3E423112;
  b = ((b << 0x16) | (b >> 0x0a)) + c;
  a = ((b & c) | (~b & d)) + *((uint32_t*)(param+0x10)) + a - 0x0A83F051;
  a = ((a << 0x07) | (a >> 0x19)) + b;
  d = ((a & b) | (~a & c)) + *((uint32_t*)(param+0x14)) + d + 0x4787C62A;
  d = ((d << 0x0c) | (d >> 0x14)) + a;
  c = ((d & a) | (~d & b)) + *((uint32_t*)(param+0x18)) + c - 0x57CFB9ED;
  c = ((c << 0x11) | (c >> 0x0f)) + d;
  b = ((c & d) | (~c & a)) + *((uint32_t*)(param+0x1c)) + b - 0x02B96AFF;
  b = ((b << 0x16) | (b >> 0x0a)) + c;
  a = ((b & c) | (~b & d)) + *((uint32_t*)(param+0x20)) + a + 0x698098D8;
  a = ((a << 0x07) | (a >> 0x19)) + b;
  d = ((a & b) | (~a & c)) + *((uint32_t*)(param+0x24)) + d - 0x74BB0851;
  d = ((d << 0x0c) | (d >> 0x14)) + a;
  c = ((d & a) | (~d & b)) + *((uint32_t*)(param+0x28)) + c - 0x0000A44F;
  c = ((c << 0x11) | (c >> 0x0f)) + d;
  b = ((c & d) | (~c & a)) + *((uint32_t*)(param+0x2C)) + b - 0x76A32842;
  b = ((b << 0x16) | (b >> 0x0a)) + c;
  a = ((b & c) | (~b & d)) + *((uint32_t*)(param+0x30)) + a + 0x6B901122;
  a = ((a << 0x07) | (a >> 0x19)) + b;
  d = ((a & b) | (~a & c)) + *((uint32_t*)(param+0x34)) + d - 0x02678E6D;
  d = ((d << 0x0c) | (d >> 0x14)) + a;
  c = ((d & a) | (~d & b)) + *((uint32_t*)(param+0x38)) + c - 0x5986BC72;
  c = ((c << 0x11) | (c >> 0x0f)) + d;
  b = ((c & d) | (~c & a)) + *((uint32_t*)(param+0x3c)) + b + 0x49B40821;
  b = ((b << 0x16) | (b >> 0x0a)) + c;
  
  a = ((b & d) | (~d & c)) + *((uint32_t*)(param+0x04)) + a - 0x09E1DA9E;
  a = ((a << 0x05) | (a >> 0x1b)) + b;
  d = ((a & c) | (~c & b)) + *((uint32_t*)(param+0x18)) + d - 0x3FBF4CC0;
  d = ((d << 0x09) | (d >> 0x17)) + a;
  c = ((d & b) | (~b & a)) + *((uint32_t*)(param+0x2c)) + c + 0x265E5A51;
  c = ((c << 0x0e) | (c >> 0x12)) + d;
  b = ((c & a) | (~a & d)) + *((uint32_t*)(param+0x00)) + b - 0x16493856;
  b = ((b << 0x14) | (b >> 0x0c)) + c;
  a = ((b & d) | (~d & c)) + *((uint32_t*)(param+0x14)) + a - 0x29D0EFA3;
  a = ((a << 0x05) | (a >> 0x1b)) + b;
  d = ((a & c) | (~c & b)) + *((uint32_t*)(param+0x28)) + d + 0x02441453;
  d = ((d << 0x09) | (d >> 0x17)) + a;
  c = ((d & b) | (~b & a)) + *((uint32_t*)(param+0x3c)) + c - 0x275E197F;
  c = ((c << 0x0e) | (c >> 0x12)) + d;
  b = ((c & a) | (~a & d)) + *((uint32_t*)(param+0x10)) + b - 0x182C0438;
  b = ((b << 0x14) | (b >> 0x0c)) + c;
  a = ((b & d) | (~d & c)) + *((uint32_t*)(param+0x24)) + a + 0x21E1CDE6;
  a = ((a << 0x05) | (a >> 0x1b)) + b;
  d = ((a & c) | (~c & b)) + *((uint32_t*)(param+0x38)) + d - 0x3CC8F82A;
  d = ((d << 0x09) | (d >> 0x17)) + a;
  c = ((d & b) | (~b & a)) + *((uint32_t*)(param+0x0c)) + c - 0x0B2AF279;
  c = ((c << 0x0e) | (c >> 0x12)) + d;
  b = ((c & a) | (~a & d)) + *((uint32_t*)(param+0x20)) + b + 0x455A14ED;
  b = ((b << 0x14) | (b >> 0x0c)) + c;
  a = ((b & d) | (~d & c)) + *((uint32_t*)(param+0x34)) + a - 0x561C16FB;
  a = ((a << 0x05) | (a >> 0x1b)) + b;
  d = ((a & c) | (~c & b)) + *((uint32_t*)(param+0x08)) + d - 0x03105C08;
  d = ((d << 0x09) | (d >> 0x17)) + a;
  c = ((d & b) | (~b & a)) + *((uint32_t*)(param+0x1c)) + c + 0x676F02D9;
  c = ((c << 0x0e) | (c >> 0x12)) + d;
  b = ((c & a) | (~a & d)) + *((uint32_t*)(param+0x30)) + b - 0x72D5B376;
  b = ((b << 0x14) | (b >> 0x0c)) + c;
  
  a = (b ^ c ^ d) + *((uint32_t*)(param+0x14)) + a - 0x0005C6BE;
  a = ((a << 0x04) | (a >> 0x1c)) + b;
  d = (a ^ b ^ c) + *((uint32_t*)(param+0x20)) + d - 0x788E097F;
  d = ((d << 0x0b) | (d >> 0x15)) + a;
  c = (d ^ a ^ b) + *((uint32_t*)(param+0x2c)) + c + 0x6D9D6122;
  c = ((c << 0x10) | (c >> 0x10)) + d;
  b = (c ^ d ^ a) + *((uint32_t*)(param+0x38)) + b - 0x021AC7F4;
  b = ((b << 0x17) | (b >> 0x09)) + c;
  a = (b ^ c ^ d) + *((uint32_t*)(param+0x04)) + a - 0x5B4115BC;
  a = ((a << 0x04) | (a >> 0x1c)) + b;
  d = (a ^ b ^ c) + *((uint32_t*)(param+0x10)) + d + 0x4BDECFA9;
  d = ((d << 0x0b) | (d >> 0x15)) + a;
  c = (d ^ a ^ b) + *((uint32_t*)(param+0x1c)) + c - 0x0944B4A0;
  c = ((c << 0x10) | (c >> 0x10)) + d;
  b = (c ^ d ^ a) + *((uint32_t*)(param+0x28)) + b - 0x41404390;
  b = ((b << 0x17) | (b >> 0x09)) + c;
  a = (b ^ c ^ d) + *((uint32_t*)(param+0x34)) + a + 0x289B7EC6;
  a = ((a << 0x04) | (a >> 0x1c)) + b;
  d = (a ^ b ^ c) + *((uint32_t*)(param+0x00)) + d - 0x155ED806;
  d = ((d << 0x0b) | (d >> 0x15)) + a;
  c = (d ^ a ^ b) + *((uint32_t*)(param+0x0c)) + c - 0x2B10CF7B;
  c = ((c << 0x10) | (c >> 0x10)) + d;
  b = (c ^ d ^ a) + *((uint32_t*)(param+0x18)) + b + 0x04881D05;
  b = ((b << 0x17) | (b >> 0x09)) + c;
  a = (b ^ c ^ d) + *((uint32_t*)(param+0x24)) + a - 0x262B2FC7;
  a = ((a << 0x04) | (a >> 0x1c)) + b;
  d = (a ^ b ^ c) + *((uint32_t*)(param+0x30)) + d - 0x1924661B;
  d = ((d << 0x0b) | (d >> 0x15)) + a;
  c = (d ^ a ^ b) + *((uint32_t*)(param+0x3c)) + c + 0x1fa27cf8;
  c = ((c << 0x10) | (c >> 0x10)) + d;
  b = (c ^ d ^ a) + *((uint32_t*)(param+0x08)) + b - 0x3B53A99B;
  b = ((b << 0x17) | (b >> 0x09)) + c;
  
  a = ((~d | b) ^ c)  + *((uint32_t*)(param+0x00)) + a - 0x0BD6DDBC;
  a = ((a << 0x06) | (a >> 0x1a)) + b; 
  d = ((~c | a) ^ b)  + *((uint32_t*)(param+0x1c)) + d + 0x432AFF97;
  d = ((d << 0x0a) | (d >> 0x16)) + a; 
  c = ((~b | d) ^ a)  + *((uint32_t*)(param+0x38)) + c - 0x546BDC59;
  c = ((c << 0x0f) | (c >> 0x11)) + d; 
  b = ((~a | c) ^ d)  + *((uint32_t*)(param+0x14)) + b - 0x036C5FC7;
  b = ((b << 0x15) | (b >> 0x0b)) + c; 
  a = ((~d | b) ^ c)  + *((uint32_t*)(param+0x30)) + a + 0x655B59C3;
  a = ((a << 0x06) | (a >> 0x1a)) + b; 
  d = ((~c | a) ^ b)  + *((uint32_t*)(param+0x0C)) + d - 0x70F3336E;
  d = ((d << 0x0a) | (d >> 0x16)) + a; 
  c = ((~b | d) ^ a)  + *((uint32_t*)(param+0x28)) + c - 0x00100B83;
  c = ((c << 0x0f) | (c >> 0x11)) + d; 
  b = ((~a | c) ^ d)  + *((uint32_t*)(param+0x04)) + b - 0x7A7BA22F;
  b = ((b << 0x15) | (b >> 0x0b)) + c; 
  a = ((~d | b) ^ c)  + *((uint32_t*)(param+0x20)) + a + 0x6FA87E4F;
  a = ((a << 0x06) | (a >> 0x1a)) + b; 
  d = ((~c | a) ^ b)  + *((uint32_t*)(param+0x3c)) + d - 0x01D31920;
  d = ((d << 0x0a) | (d >> 0x16)) + a; 
  c = ((~b | d) ^ a)  + *((uint32_t*)(param+0x18)) + c - 0x5CFEBCEC;
  c = ((c << 0x0f) | (c >> 0x11)) + d; 
  b = ((~a | c) ^ d)  + *((uint32_t*)(param+0x34)) + b + 0x4E0811A1;
  b = ((b << 0x15) | (b >> 0x0b)) + c; 
  a = ((~d | b) ^ c)  + *((uint32_t*)(param+0x10)) + a - 0x08AC817E;
  a = ((a << 0x06) | (a >> 0x1a)) + b; 
  d = ((~c | a) ^ b)  + *((uint32_t*)(param+0x2c)) + d - 0x42C50DCB;
  d = ((d << 0x0a) | (d >> 0x16)) + a; 
  c = ((~b | d) ^ a)  + *((uint32_t*)(param+0x08)) + c + 0x2AD7D2BB;
  c = ((c << 0x0f) | (c >> 0x11)) + d; 
  b = ((~a | c) ^ d)  + *((uint32_t*)(param+0x24)) + b - 0x14792C6F;
  b = ((b << 0x15) | (b >> 0x0b)) + c; 

#ifdef LOG
  printf("real: hash output: %x %x %x %x\n", a, b, c, d);
#endif
  
  *((uint32_t *)(field+0)) += a;
  *((uint32_t *)(field+4)) += b;
  *((uint32_t *)(field+8)) += c;
  *((uint32_t *)(field+12)) += d;
}

static void call_hash (char *key, char *challenge, int len) {

  uint32_t *ptr1, *ptr2;
  uint32_t a, b, c, d;
  
  ptr1=(uint32_t*)(key+16);
  ptr2=(uint32_t*)(key+20);
  
  a = *ptr1;
  b = (a >> 3) & 0x3f;
  a += len * 8;
  *ptr1 = a;
  
  if (a < (len << 3))
  {
#ifdef LOG
    printf("not verified: (len << 3) > a true\n");
#endif
    ptr2 += 4;
  }

  *ptr2 += (len >> 0x1d);
  a = 64 - b;
  c = 0;  
  if (a <= len)
  {

    memcpy(key+b+24, challenge, a);
    hash(key, key+24);
    c = a;
    d = c + 0x3f;
    
    while ( d < len ) {

#ifdef LOG
      printf("not verified:  while ( d < len )\n");
#endif
      hash(key, challenge+d-0x3f);
      d += 64;
      c += 64;
    }
    b = 0;
  }
  
  memcpy(key+b+24, challenge+c, len-c);
}

static void calc_response (char *result, char *field) {

  char buf1[128];
  char buf2[128];
  int i;

  memset (buf1, 0, 64);
  *buf1 = 128;
  
  memcpy (buf2, field+16, 8);
  
  i = ( *((uint32_t*)(buf2)) >> 3 ) & 0x3f;
 
  if (i < 56) {
    i = 56 - i;
  } else {
#ifdef LOG
    printf("not verified: ! (i < 56)\n");
#endif
    i = 120 - i;
  }

  call_hash (field, buf1, i);
  call_hash (field, buf2, 8);

  memcpy (result, field, 16);

}


static void calc_response_string (char *result, char *challenge) {
 
  char field[128];
  char zres[20];
  int  i;
      
  /* initialize our field */
  BE_32C (field,      0x01234567);
  BE_32C ((field+4),  0x89ABCDEF);
  BE_32C ((field+8),  0xFEDCBA98);
  BE_32C ((field+12), 0x76543210);
  BE_32C ((field+16), 0x00000000);
  BE_32C ((field+20), 0x00000000);

  /* calculate response */
  call_hash(field, challenge, 64);
  calc_response(zres,field);
 
  /* convert zres to ascii string */
  for (i=0; i<16; i++ ) {
    char a, b;
    
    a = (zres[i] >> 4) & 15;
    b = zres[i] & 15;

    result[i*2]   = ((a<10) ? (a+48) : (a+87)) & 255;
    result[i*2+1] = ((b<10) ? (b+48) : (b+87)) & 255;
  }
}

void real_calc_response_and_checksum (char *response, char *chksum, char *challenge) {

  int   ch_len, table_len, resp_len;
  int   i;
  char *ptr;
  char  buf[128];

  /* initialize return values */
  memset(response, 0, 64);
  memset(chksum, 0, 34);

  /* initialize buffer */
  memset(buf, 0, 128);
  ptr=buf;
  BE_32C(ptr, 0xa1e9149d);
  ptr+=4;
  BE_32C(ptr, 0x0e6b3b59);
  ptr+=4;

  /* some (length) checks */
  if (challenge != NULL)
  {
    ch_len = strlen (challenge);

    if (ch_len == 40) /* what a hack... */
    {
      challenge[32]=0;
      ch_len=32;
    }
    if ( ch_len > 56 ) ch_len=56;
    
    /* copy challenge to buf */
    memcpy(ptr, challenge, ch_len);
  }
  
  if (xor_table != NULL)
  {
    table_len = strlen(xor_table);

    if (table_len > 56) table_len=56;

    /* xor challenge bytewise with xor_table */
    for (i=0; i<table_len; i++)
      ptr[i] = ptr[i] ^ xor_table[i];
  }

  calc_response_string (response, buf);

  /* add tail */
  resp_len = strlen (response);
  strcpy (&response[resp_len], "01d0a8e3");

  /* calculate checksum */
  for (i=0; i<resp_len/4; i++)
    chksum[i] = response[i*4];
}


/*
 * takes a MLTI-Chunk and a rule number got from match_asm_rule,
 * returns a pointer to selected data and number of bytes in that.
 */

static int select_mlti_data(const char *mlti_chunk, int selection, char *out) {

  int numrules, codec, size;
  int i;
  
  /* MLTI chunk should begin with MLTI */

  if ((mlti_chunk[0] != 'M')
      ||(mlti_chunk[1] != 'L')
      ||(mlti_chunk[2] != 'T')
      ||(mlti_chunk[3] != 'I'))
  {
    printf("libreal: MLTI tag not detected\n");
    return 0;
  }

  mlti_chunk+=4;

  /* next 16 bits are the number of rules */
  numrules=BE_16(mlti_chunk);
  if (selection >= numrules) return 0;

  /* now <numrules> indices of codecs follows */
  /* we skip to selection                     */
  mlti_chunk+=(selection+1)*2;

  /* get our index */
  codec=BE_16(mlti_chunk);

  /* skip to number of codecs */
  mlti_chunk+=(numrules-selection)*2;

  /* get number of codecs */
  numrules=BE_16(mlti_chunk);

  if (codec >= numrules) {
    printf("codec index >= number of codecs. %i %i\n", codec, numrules);
    return 0;
  }

  mlti_chunk+=2;
 
  /* now seek to selected codec */
  for (i=0; i<codec; i++) {
    size=BE_32(mlti_chunk);
    mlti_chunk+=size+4;
  }
  
  size=BE_32(mlti_chunk);

#ifdef LOG
  hexdump(mlti_chunk+4, size);
#endif
  memcpy(out,mlti_chunk+4, size);
  return size;
}

/*
 * Decodes base64 strings (based upon b64 package)
 */

static char *b64_decode(const char *in)
{
  char dtable[256];              /* Encode / decode table */
  int i,j,k;
  char *out=malloc(sizeof(char)*strlen(in));

  for (i = 0; i < 255; i++) {
    dtable[i] = 0x80;
  }
  for (i = 'A'; i <= 'Z'; i++) {
    dtable[i] = 0 + (i - 'A');
  }
  for (i = 'a'; i <= 'z'; i++) {
    dtable[i] = 26 + (i - 'a');
  }
  for (i = '0'; i <= '9'; i++) {
    dtable[i] = 52 + (i - '0');
  }
  dtable['+'] = 62;
  dtable['/'] = 63;
  dtable['='] = 0;

  k=0;
  
  /*CONSTANTCONDITION*/
  for (j=0; j<strlen(in); j+=4)
  {
    char a[4], b[4];

    for (i = 0; i < 4; i++) {
      int c = in[i+j];

      if (dtable[c] & 0x80) {
        printf("Illegal character '%c' in input.\n", c);
        exit(1);
      }
      a[i] = (char) c;
      b[i] = (char) dtable[c];
    }
    out[k++] = (b[0] << 2) | (b[1] >> 4);
    out[k++] = (b[1] << 4) | (b[2] >> 2);
    out[k++] = (b[2] << 6) | b[3];
    i = a[2] == '=' ? 1 : (a[3] == '=' ? 2 : 3);
    if (i < 3) {
      out[k]=0;
      return out;
    }
  }
  out[k]=0;
  return out;
}

/*
 * looking at stream description.
 */
 
static int sdp_filter(const char *in, const char *filter, char *out) {

  int flen=strlen(filter);
  int len=strchr(in,'\n')-in;

  if (!strncmp(in,filter,flen))
  {
    if(in[flen]=='"') flen++;
    if(in[len-1]=='"') len--;
    memcpy(out, &in[flen], len-flen);
    out[len-flen]=0;

    return len-flen;
  }
  
  return 0;
}

rmff_header_t *real_parse_sdp(const char *data, char *stream_rules, uint32_t bandwidth) {

  rmff_header_t *h=malloc(sizeof(rmff_header_t));
  rmff_mdpr_t   *media=NULL;
  char buf[4096];
  int rulematches[16];
  int n;
  int len;
  int have_audio=0, have_video=0, stream, sr[2];

  stream_rules[0]=0;
  
  h=malloc(sizeof(rmff_header_t));
  h->streams=malloc(sizeof(rmff_mdpr_t*)*3);
  h->streams[0]=NULL;
  h->streams[1]=NULL;
  h->streams[2]=NULL;
  h->prop=malloc(sizeof(rmff_prop_t));
  h->streams[0]=malloc(sizeof(rmff_mdpr_t));
  h->streams[1]=malloc(sizeof(rmff_mdpr_t));
  h->cont=malloc(sizeof(rmff_cont_t));
  h->data=malloc(sizeof(rmff_data_t));
  h->fileheader=malloc(sizeof(rmff_fileheader_t));
  h->fileheader->object_id=RMF_TAG;
  h->fileheader->size=18;
  h->fileheader->object_version=0;
  h->fileheader->file_version=0;
  h->fileheader->num_headers=6;

  h->prop->object_version=0;
  h->streams[0]->object_version=0;
  h->streams[1]->object_version=0;
  h->cont->object_version=0;
  h->data->object_version=0;

  h->prop->object_id=PROP_TAG;
  h->streams[0]->object_id=MDPR_TAG;
  h->streams[1]->object_id=MDPR_TAG;
  h->cont->object_id=CONT_TAG;
  h->data->object_id=DATA_TAG;

  h->prop->size=50;
  h->data->size=18;
  h->data->next_data_header=0;

  h->cont->title=NULL;
  h->cont->author=NULL;
  h->cont->copyright=NULL;
  h->cont->comment=NULL;

  while (*data) {

    if(sdp_filter(data,"m=audio",buf))
    {
      media=h->streams[0];
      have_audio=1;
      stream=0;
    }
    if(sdp_filter(data,"m=video",buf))
    {
      media=h->streams[1];
      have_video=1;
      stream=1;
    }
  
    /* cont stuff */
  
    len=sdp_filter(data,"a=Title:buffer;",buf);
    if (len) h->cont->title=b64_decode(buf);
    
    len=sdp_filter(data,"a=Author:buffer;",buf);
    if (len) h->cont->author=b64_decode(buf);
    
    len=sdp_filter(data,"a=Copyright:buffer;",buf);
    if (len) h->cont->copyright=b64_decode(buf);
    
    len=sdp_filter(data,"a=Abstract:buffer;",buf);
    if (len) h->cont->comment=b64_decode(buf);

    /* prop stuff */

    len=sdp_filter(data,"a=StreamCount:integer;",buf);
    if (len) h->prop->num_streams=atoi(buf);

    len=sdp_filter(data,"a=Flags:integer;",buf);
    if (len) h->prop->flags=atoi(buf);

    /* mdpr stuff */

    if (media) {

      len=sdp_filter(data,"a=control:streamid=",buf);
      if (len) media->stream_number=atoi(buf);

      len=sdp_filter(data,"a=MaxBitRate:integer;",buf);
      if (len)
      {
        media->max_bit_rate=atoi(buf);
        media->avg_bit_rate=media->max_bit_rate;
      }

      len=sdp_filter(data,"a=MaxPacketSize:integer;",buf);
      if (len)
      {
        media->max_packet_size=atoi(buf);
        media->avg_packet_size=media->max_packet_size;
      }
      
      len=sdp_filter(data,"a=StartTime:integer;",buf);
      if (len) media->start_time=atoi(buf);

      len=sdp_filter(data,"a=Preroll:integer;",buf);
      if (len) media->preroll=atoi(buf);

      len=sdp_filter(data,"a=length:npt=",buf);
      if (len) media->duration=(uint32_t)(atof(buf)*1000);

      len=sdp_filter(data,"a=StreamName:string;",buf);
      if (len)
      {
        media->stream_name=strdup(buf);
        media->stream_name_size=strlen(media->stream_name);
      }
      len=sdp_filter(data,"a=mimetype:string;",buf);
      if (len)
      {
        media->mime_type=strdup(buf);
        media->mime_type_size=strlen(media->mime_type);
      }
      len=sdp_filter(data,"a=OpaqueData:buffer;",buf);
      if (len)
      {
        media->mlti_data=b64_decode(buf);
      }
      len=sdp_filter(data,"a=ASMRuleBook:string;",buf);
      if (len)
      {
        int i=0;
        char b[64];

#ifdef LOG
        printf("calling asmrp_match with:\n%s\n%u\n", buf, bandwidth);
#endif
        n=asmrp_match(buf, bandwidth, rulematches);
        sr[stream]=rulematches[0];
        for (i=0; i<n; i++) {
#ifdef LOG
          printf("asmrp rule match: %u for stream %u\n", rulematches[i], stream);
#endif
          sprintf(b,"stream=%u;rule=%u,", stream, rulematches[i]);
          strcat(stream_rules, b);
        }
      }
    }
    data+=strchr(data,'\n')-data+1;
  }
  if (strlen(stream_rules)>0);
    stream_rules[strlen(stream_rules)-1]=0; /* delete last , */

  /* we have to reconstruct some data in prop */
  h->prop->max_bit_rate=0;
  h->prop->avg_bit_rate=0;
  h->prop->max_packet_size=0;
  h->prop->avg_packet_size=0;
  h->prop->num_packets=0;
  h->prop->duration=0;
  h->prop->preroll=0;
  h->prop->index_offset=0;
  
      
  
  if(have_video)
  {
  
    h->prop->max_bit_rate=h->streams[1]->max_bit_rate;
    h->prop->avg_bit_rate=h->streams[1]->avg_bit_rate;
    h->prop->max_packet_size=h->streams[1]->max_packet_size;
    h->prop->avg_packet_size=h->streams[1]->avg_packet_size;
    h->prop->duration=h->streams[1]->duration;
    /* h->prop->preroll=h->streams[1]->preroll; */

    /* select a codec */
    h->streams[1]->type_specific_len=select_mlti_data(
          h->streams[1]->mlti_data, sr[1], buf);
    h->streams[1]->type_specific_data=malloc(sizeof(char)*h->streams[1]->type_specific_len);
    memcpy(h->streams[1]->type_specific_data, buf, h->streams[1]->type_specific_len);
    
  } else
  {
    free(h->streams[1]);
    h->streams[1]=NULL;
  }
  if(have_audio)
  {
  
    h->prop->max_bit_rate+=h->streams[0]->max_bit_rate;
    h->prop->avg_bit_rate+=h->streams[0]->avg_bit_rate;
    h->prop->max_packet_size=MAX(h->prop->max_packet_size,
        h->streams[0]->max_packet_size);
    if (have_video)
      h->prop->avg_packet_size=(h->streams[1]->avg_packet_size
          +h->streams[0]->avg_packet_size) / 2;
    else
      h->prop->avg_packet_size=h->streams[0]->avg_packet_size;
    h->prop->duration=MAX(h->prop->duration,h->streams[0]->duration);
    /* h->prop->preroll=MAX(h->streams[1]->preroll,h->streams[0]->preroll); */
 
    /* select a codec */
    h->streams[0]->type_specific_len=select_mlti_data(
          h->streams[0]->mlti_data, sr[0], buf);
    h->streams[0]->type_specific_data=malloc(sizeof(char)*h->streams[0]->type_specific_len);
    memcpy(h->streams[0]->type_specific_data, buf, h->streams[0]->type_specific_len);
    
 } else
  {
    free(h->streams[0]);
    h->streams[0]=NULL;
  }

  /* fix sizes */
      
  h->cont->title_len=0;
  h->cont->author_len=0;
  h->cont->copyright_len=0;
  h->cont->comment_len=0;
  if (h->cont->title) h->cont->title_len=strlen(h->cont->title);
  if (h->cont->author) h->cont->author_len=strlen(h->cont->author);
  if (h->cont->copyright) h->cont->copyright_len=strlen(h->cont->copyright);
  if (h->cont->comment) h->cont->comment_len=strlen(h->cont->comment);

  h->cont->size=18
      +h->cont->title_len
      +h->cont->author_len
      +h->cont->copyright_len
      +h->cont->comment_len;

  if (have_audio)
    h->streams[0]->size=12+7*4+6
      +h->streams[0]->stream_name_size
      +h->streams[0]->mime_type_size
      +h->streams[0]->type_specific_len;

  if (have_video)
    h->streams[1]->size=12+7*4+6
      +h->streams[1]->stream_name_size
      +h->streams[1]->mime_type_size
      +h->streams[1]->type_specific_len;

  /* fix data offset */
  
  h->prop->data_offset=RMFF_HEADER_SIZE
      +h->cont->size
      +h->prop->size;

  if (have_video)
    h->prop->data_offset+=h->streams[1]->size;

  if (have_audio)
    h->prop->data_offset+=h->streams[0]->size;

  return h;
}

int real_get_rdt_chunk(rtsp_t *rtsp_session, char *buffer) {

  int n=1;
  uint8_t header[8];
  rmff_pheader_t ph;
  int size;
  int flags1;
  int unknown1;
  uint32_t ts;

  n=rtsp_read_data(rtsp_session, header, 8);
  if (n<8) return 0;
  if (header[0] != 0x24)
  {
    printf("rdt chunk not recognized: got 0x%02x\n", header[0]);
    return 0;
  }
  size=(header[1]<<12)+(header[2]<<8)+(header[3]);
  flags1=header[4];
  if ((flags1!=0x40)&&(flags1!=0x42))
  {
#ifdef LOG
    printf("got flags1: 0x%02x\n",flags1);
#endif
    header[0]=header[5];
    header[1]=header[6];
    header[2]=header[7];
    n=rtsp_read_data(rtsp_session, header+3, 5);
    if (n<5) return 0;
#ifdef LOG
    printf("ignoring bytes:\n");
    hexdump(header, 8);
#endif
    n=rtsp_read_data(rtsp_session, header+4, 4);
    if (n<4) return 0;
    flags1=header[4];
    size-=9;
  }
  unknown1=(header[5]<<12)+(header[6]<<8)+(header[7]);
  n=rtsp_read_data(rtsp_session, header, 6);
  if (n<6) return 0;
  ts=BE_32(header);
  
#ifdef LOG
  printf("ts: %u size: %u, flags: 0x%02x, unknown values: %u 0x%02x 0x%02x\n", 
          ts, size, flags1, unknown1, header[4], header[5]);
#endif
  size+=2;
  
  ph.object_version=0;
  ph.length=size;
  ph.stream_number=(flags1>>1)&1;
  ph.timestamp=ts;
  ph.reserved=0;
  ph.flags=0;      /* TODO: determine keyframe flag and insert here? */
  rmff_dump_pheader(&ph, buffer);
  size-=12;
  n=rtsp_read_data(rtsp_session, buffer+12, size);
  
  return n+12;
}

rmff_header_t  *real_setup_and_get_header(rtsp_t *rtsp_session, uint32_t bandwidth) {

  char *description=NULL;
  char *session_id=NULL;
  rmff_header_t *h;
  char *challenge1;
  char challenge2[64];
  char checksum[34];
  char subscribe[256];
  char buf[256];
  char *mrl=rtsp_get_mrl(rtsp_session);
  unsigned int size;
  int status;
  
  /* get challenge */
  challenge1=strdup(rtsp_search_answers(rtsp_session,"RealChallenge1"));
#ifdef LOG
  printf("real: Challenge1: %s\n", challenge1);
#endif
  
  /* request stream description */
  rtsp_schedule_field(rtsp_session, "Accept: application/sdp");
  sprintf(buf, "Bandwidth: %u", bandwidth);
  rtsp_schedule_field(rtsp_session, buf);
  rtsp_schedule_field(rtsp_session, "Require: com.real.retain-entity-for-setup");
  status=rtsp_request_describe(rtsp_session,NULL);

  if (status != 200) return NULL;

  /* receive description */
  size=0;
  if (!rtsp_search_answers(rtsp_session,"Content-length"))
    printf("real: got no Content-length!\n");
  else
    size=atoi(rtsp_search_answers(rtsp_session,"Content-length"));

  if (!rtsp_search_answers(rtsp_session,"ETag"))
    printf("real: got no ETag!\n");
  else
    session_id=strdup(rtsp_search_answers(rtsp_session,"ETag"));
    
#ifdef LOG
  printf("real: Stream description size: %i\n", size);
#endif

  description=malloc(sizeof(char)*(size+1));

  rtsp_read_data(rtsp_session, description, size);
  description[size]=0;

  /* parse sdp (sdpplin) and create a header and a subscribe string */
  strcpy(subscribe, "Subscribe: ");
  h=real_parse_sdp(description, subscribe+11, bandwidth);
  rmff_fix_header(h);

#ifdef LOG
  printf("Title: %s\nCopyright: %s\nAuthor: %s\nStreams: %i\n",
    h->cont->title, h->cont->copyright, h->cont->author, h->prop->num_streams);
#endif
  
  /* setup our streams */
  real_calc_response_and_checksum (challenge2, checksum, challenge1);
  sprintf(buf, "RealChallenge2: %s, sd=%s", challenge2, checksum);
  rtsp_schedule_field(rtsp_session, buf);
  sprintf(buf, "If-Match: %s", session_id);
  rtsp_schedule_field(rtsp_session, buf);
  rtsp_schedule_field(rtsp_session, "Transport: x-pn-tng/tcp;mode=play,rtp/avp/tcp;unicast;mode=play");
  sprintf(buf, "%s/streamid=0", mrl);
  rtsp_request_setup(rtsp_session,buf);

  if (h->prop->num_streams > 1) {
    rtsp_schedule_field(rtsp_session, "Transport: x-pn-tng/tcp;mode=play,rtp/avp/tcp;unicast;mode=play");
    sprintf(buf, "If-Match: %s", session_id);
    rtsp_schedule_field(rtsp_session, buf);

    sprintf(buf, "%s/streamid=1", mrl);
    rtsp_request_setup(rtsp_session,buf);
  }
  /* set stream parameter (bandwidth) with our subscribe string */
  rtsp_schedule_field(rtsp_session, subscribe);
  rtsp_request_setparameter(rtsp_session,NULL);

  /* and finally send a play request */
  rtsp_schedule_field(rtsp_session, "Range: npt=0-");
  rtsp_request_play(rtsp_session,NULL);

  return h;
}

