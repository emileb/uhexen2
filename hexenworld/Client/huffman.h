//	huffman.h
//	huffman encoding/decoding for use in hexenworld networking

#ifndef __H2W_HUFFMAN_H
#define __H2W_HUFFMAN_H

extern void HuffInit (void);
extern void HuffEncode (unsigned char *in, unsigned char *out, int inlen, int *outlen);
extern void HuffDecode (unsigned char *in, unsigned char *out, int inlen, int *outlen, const int maxlen);

#ifdef DEBUG_BUILD
extern void ZeroFreq (void);
extern void CalcFreq (unsigned char *packet, int packetlen);
extern void PrintFreqs (void);

extern	int	HuffIn;
extern	int	HuffOut;
#endif

#endif	/* __H2W_HUFFMAN_H */

