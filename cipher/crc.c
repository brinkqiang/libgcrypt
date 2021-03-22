/* crc.c - Cyclic redundancy checks.
 * Copyright (C) 2003 Free Software Foundation, Inc.
 *
 * This file is part of Libgcrypt.
 *
 * Libgcrypt is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * Libgcrypt is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 *
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "g10lib.h"
#include "cipher.h"

#include "bithelp.h"
#include "bufhelp.h"


/* USE_INTEL_PCLMUL indicates whether to compile CRC with Intel PCLMUL/SSE4.1
 * code.  */
#undef USE_INTEL_PCLMUL
#if defined(ENABLE_PCLMUL_SUPPORT) && defined(ENABLE_SSE41_SUPPORT)
# if ((defined(__i386__) && SIZEOF_UNSIGNED_LONG == 4) || defined(__x86_64__))
#  if __GNUC__ >= 4
#   define USE_INTEL_PCLMUL 1
#  endif
# endif
#endif /* USE_INTEL_PCLMUL */

/* USE_ARM_PMULL indicates whether to compile GCM with ARMv8 PMULL code. */
#undef USE_ARM_PMULL
#if defined(ENABLE_ARM_CRYPTO_SUPPORT)
# if defined(__AARCH64EL__) && \
    defined(HAVE_COMPATIBLE_GCC_AARCH64_PLATFORM_AS) && \
    defined(HAVE_GCC_INLINE_ASM_AARCH64_CRYPTO)
#  define USE_ARM_PMULL 1
# endif
#endif /* USE_ARM_PMULL */

/* USE_PPC_VPMSUM indicates whether to enable PowerPC vector
 * accelerated code. */
#undef USE_PPC_VPMSUM
#ifdef ENABLE_PPC_CRYPTO_SUPPORT
# if defined(HAVE_COMPATIBLE_CC_PPC_ALTIVEC) && \
     defined(HAVE_GCC_INLINE_ASM_PPC_ALTIVEC)
#  if __GNUC__ >= 4
#   define USE_PPC_VPMSUM 1
#  endif
# endif
#endif /* USE_PPC_VPMSUM */


typedef struct
{
  u32 CRC;
#ifdef USE_INTEL_PCLMUL
  unsigned int use_pclmul:1;           /* Intel PCLMUL shall be used.  */
#endif
#ifdef USE_ARM_PMULL
  unsigned int use_pmull:1;            /* ARMv8 PMULL shall be used. */
#endif
#ifdef USE_PPC_VPMSUM
  unsigned int use_vpmsum:1;           /* POWER vpmsum shall be used. */
#endif
  byte buf[4];
}
CRC_CONTEXT;


#ifdef USE_INTEL_PCLMUL
/*-- crc-intel-pclmul.c --*/
void _gcry_crc32_intel_pclmul (u32 *pcrc, const byte *inbuf, size_t inlen);
void _gcry_crc24rfc2440_intel_pclmul (u32 *pcrc, const byte *inbuf,
				      size_t inlen);
#endif

#ifdef USE_ARM_PMULL
/*-- crc-armv8-ce.c --*/
void _gcry_crc32_armv8_ce_pmull (u32 *pcrc, const byte *inbuf, size_t inlen);
void _gcry_crc24rfc2440_armv8_ce_pmull (u32 *pcrc, const byte *inbuf,
					size_t inlen);
#endif

#ifdef USE_PPC_VPMSUM
/*-- crc-ppc.c --*/
void _gcry_crc32_ppc8_vpmsum (u32 *pcrc, const byte *inbuf, size_t inlen);
void _gcry_crc24rfc2440_ppc8_vpmsum (u32 *pcrc, const byte *inbuf,
				     size_t inlen);
#endif


/*
 * Code generated by universal_crc by Danjel McGougan
 *
 * CRC parameters used:
 *   bits:       32
 *   poly:       0x04c11db7
 *   init:       0xffffffff
 *   xor:        0xffffffff
 *   reverse:    true
 *   non-direct: false
 *
 * CRC of the string "123456789" is 0xcbf43926
 */

static const u32 crc32_table[1024] = {
  0x00000000, 0x77073096, 0xee0e612c, 0x990951ba,
  0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
  0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
  0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
  0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de,
  0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
  0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,
  0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
  0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
  0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
  0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940,
  0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
  0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116,
  0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
  0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
  0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
  0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a,
  0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
  0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818,
  0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
  0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
  0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
  0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c,
  0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
  0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2,
  0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
  0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
  0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
  0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086,
  0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
  0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4,
  0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
  0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
  0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
  0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,
  0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
  0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe,
  0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
  0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
  0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
  0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252,
  0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
  0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60,
  0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
  0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
  0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
  0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04,
  0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
  0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a,
  0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
  0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
  0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
  0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e,
  0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
  0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c,
  0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
  0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
  0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
  0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0,
  0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
  0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6,
  0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
  0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
  0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d,
  0x00000000, 0x191b3141, 0x32366282, 0x2b2d53c3,
  0x646cc504, 0x7d77f445, 0x565aa786, 0x4f4196c7,
  0xc8d98a08, 0xd1c2bb49, 0xfaefe88a, 0xe3f4d9cb,
  0xacb54f0c, 0xb5ae7e4d, 0x9e832d8e, 0x87981ccf,
  0x4ac21251, 0x53d92310, 0x78f470d3, 0x61ef4192,
  0x2eaed755, 0x37b5e614, 0x1c98b5d7, 0x05838496,
  0x821b9859, 0x9b00a918, 0xb02dfadb, 0xa936cb9a,
  0xe6775d5d, 0xff6c6c1c, 0xd4413fdf, 0xcd5a0e9e,
  0x958424a2, 0x8c9f15e3, 0xa7b24620, 0xbea97761,
  0xf1e8e1a6, 0xe8f3d0e7, 0xc3de8324, 0xdac5b265,
  0x5d5daeaa, 0x44469feb, 0x6f6bcc28, 0x7670fd69,
  0x39316bae, 0x202a5aef, 0x0b07092c, 0x121c386d,
  0xdf4636f3, 0xc65d07b2, 0xed705471, 0xf46b6530,
  0xbb2af3f7, 0xa231c2b6, 0x891c9175, 0x9007a034,
  0x179fbcfb, 0x0e848dba, 0x25a9de79, 0x3cb2ef38,
  0x73f379ff, 0x6ae848be, 0x41c51b7d, 0x58de2a3c,
  0xf0794f05, 0xe9627e44, 0xc24f2d87, 0xdb541cc6,
  0x94158a01, 0x8d0ebb40, 0xa623e883, 0xbf38d9c2,
  0x38a0c50d, 0x21bbf44c, 0x0a96a78f, 0x138d96ce,
  0x5ccc0009, 0x45d73148, 0x6efa628b, 0x77e153ca,
  0xbabb5d54, 0xa3a06c15, 0x888d3fd6, 0x91960e97,
  0xded79850, 0xc7cca911, 0xece1fad2, 0xf5facb93,
  0x7262d75c, 0x6b79e61d, 0x4054b5de, 0x594f849f,
  0x160e1258, 0x0f152319, 0x243870da, 0x3d23419b,
  0x65fd6ba7, 0x7ce65ae6, 0x57cb0925, 0x4ed03864,
  0x0191aea3, 0x188a9fe2, 0x33a7cc21, 0x2abcfd60,
  0xad24e1af, 0xb43fd0ee, 0x9f12832d, 0x8609b26c,
  0xc94824ab, 0xd05315ea, 0xfb7e4629, 0xe2657768,
  0x2f3f79f6, 0x362448b7, 0x1d091b74, 0x04122a35,
  0x4b53bcf2, 0x52488db3, 0x7965de70, 0x607eef31,
  0xe7e6f3fe, 0xfefdc2bf, 0xd5d0917c, 0xcccba03d,
  0x838a36fa, 0x9a9107bb, 0xb1bc5478, 0xa8a76539,
  0x3b83984b, 0x2298a90a, 0x09b5fac9, 0x10aecb88,
  0x5fef5d4f, 0x46f46c0e, 0x6dd93fcd, 0x74c20e8c,
  0xf35a1243, 0xea412302, 0xc16c70c1, 0xd8774180,
  0x9736d747, 0x8e2de606, 0xa500b5c5, 0xbc1b8484,
  0x71418a1a, 0x685abb5b, 0x4377e898, 0x5a6cd9d9,
  0x152d4f1e, 0x0c367e5f, 0x271b2d9c, 0x3e001cdd,
  0xb9980012, 0xa0833153, 0x8bae6290, 0x92b553d1,
  0xddf4c516, 0xc4eff457, 0xefc2a794, 0xf6d996d5,
  0xae07bce9, 0xb71c8da8, 0x9c31de6b, 0x852aef2a,
  0xca6b79ed, 0xd37048ac, 0xf85d1b6f, 0xe1462a2e,
  0x66de36e1, 0x7fc507a0, 0x54e85463, 0x4df36522,
  0x02b2f3e5, 0x1ba9c2a4, 0x30849167, 0x299fa026,
  0xe4c5aeb8, 0xfdde9ff9, 0xd6f3cc3a, 0xcfe8fd7b,
  0x80a96bbc, 0x99b25afd, 0xb29f093e, 0xab84387f,
  0x2c1c24b0, 0x350715f1, 0x1e2a4632, 0x07317773,
  0x4870e1b4, 0x516bd0f5, 0x7a468336, 0x635db277,
  0xcbfad74e, 0xd2e1e60f, 0xf9ccb5cc, 0xe0d7848d,
  0xaf96124a, 0xb68d230b, 0x9da070c8, 0x84bb4189,
  0x03235d46, 0x1a386c07, 0x31153fc4, 0x280e0e85,
  0x674f9842, 0x7e54a903, 0x5579fac0, 0x4c62cb81,
  0x8138c51f, 0x9823f45e, 0xb30ea79d, 0xaa1596dc,
  0xe554001b, 0xfc4f315a, 0xd7626299, 0xce7953d8,
  0x49e14f17, 0x50fa7e56, 0x7bd72d95, 0x62cc1cd4,
  0x2d8d8a13, 0x3496bb52, 0x1fbbe891, 0x06a0d9d0,
  0x5e7ef3ec, 0x4765c2ad, 0x6c48916e, 0x7553a02f,
  0x3a1236e8, 0x230907a9, 0x0824546a, 0x113f652b,
  0x96a779e4, 0x8fbc48a5, 0xa4911b66, 0xbd8a2a27,
  0xf2cbbce0, 0xebd08da1, 0xc0fdde62, 0xd9e6ef23,
  0x14bce1bd, 0x0da7d0fc, 0x268a833f, 0x3f91b27e,
  0x70d024b9, 0x69cb15f8, 0x42e6463b, 0x5bfd777a,
  0xdc656bb5, 0xc57e5af4, 0xee530937, 0xf7483876,
  0xb809aeb1, 0xa1129ff0, 0x8a3fcc33, 0x9324fd72,
  0x00000000, 0x01c26a37, 0x0384d46e, 0x0246be59,
  0x0709a8dc, 0x06cbc2eb, 0x048d7cb2, 0x054f1685,
  0x0e1351b8, 0x0fd13b8f, 0x0d9785d6, 0x0c55efe1,
  0x091af964, 0x08d89353, 0x0a9e2d0a, 0x0b5c473d,
  0x1c26a370, 0x1de4c947, 0x1fa2771e, 0x1e601d29,
  0x1b2f0bac, 0x1aed619b, 0x18abdfc2, 0x1969b5f5,
  0x1235f2c8, 0x13f798ff, 0x11b126a6, 0x10734c91,
  0x153c5a14, 0x14fe3023, 0x16b88e7a, 0x177ae44d,
  0x384d46e0, 0x398f2cd7, 0x3bc9928e, 0x3a0bf8b9,
  0x3f44ee3c, 0x3e86840b, 0x3cc03a52, 0x3d025065,
  0x365e1758, 0x379c7d6f, 0x35dac336, 0x3418a901,
  0x3157bf84, 0x3095d5b3, 0x32d36bea, 0x331101dd,
  0x246be590, 0x25a98fa7, 0x27ef31fe, 0x262d5bc9,
  0x23624d4c, 0x22a0277b, 0x20e69922, 0x2124f315,
  0x2a78b428, 0x2bbade1f, 0x29fc6046, 0x283e0a71,
  0x2d711cf4, 0x2cb376c3, 0x2ef5c89a, 0x2f37a2ad,
  0x709a8dc0, 0x7158e7f7, 0x731e59ae, 0x72dc3399,
  0x7793251c, 0x76514f2b, 0x7417f172, 0x75d59b45,
  0x7e89dc78, 0x7f4bb64f, 0x7d0d0816, 0x7ccf6221,
  0x798074a4, 0x78421e93, 0x7a04a0ca, 0x7bc6cafd,
  0x6cbc2eb0, 0x6d7e4487, 0x6f38fade, 0x6efa90e9,
  0x6bb5866c, 0x6a77ec5b, 0x68315202, 0x69f33835,
  0x62af7f08, 0x636d153f, 0x612bab66, 0x60e9c151,
  0x65a6d7d4, 0x6464bde3, 0x662203ba, 0x67e0698d,
  0x48d7cb20, 0x4915a117, 0x4b531f4e, 0x4a917579,
  0x4fde63fc, 0x4e1c09cb, 0x4c5ab792, 0x4d98dda5,
  0x46c49a98, 0x4706f0af, 0x45404ef6, 0x448224c1,
  0x41cd3244, 0x400f5873, 0x4249e62a, 0x438b8c1d,
  0x54f16850, 0x55330267, 0x5775bc3e, 0x56b7d609,
  0x53f8c08c, 0x523aaabb, 0x507c14e2, 0x51be7ed5,
  0x5ae239e8, 0x5b2053df, 0x5966ed86, 0x58a487b1,
  0x5deb9134, 0x5c29fb03, 0x5e6f455a, 0x5fad2f6d,
  0xe1351b80, 0xe0f771b7, 0xe2b1cfee, 0xe373a5d9,
  0xe63cb35c, 0xe7fed96b, 0xe5b86732, 0xe47a0d05,
  0xef264a38, 0xeee4200f, 0xeca29e56, 0xed60f461,
  0xe82fe2e4, 0xe9ed88d3, 0xebab368a, 0xea695cbd,
  0xfd13b8f0, 0xfcd1d2c7, 0xfe976c9e, 0xff5506a9,
  0xfa1a102c, 0xfbd87a1b, 0xf99ec442, 0xf85cae75,
  0xf300e948, 0xf2c2837f, 0xf0843d26, 0xf1465711,
  0xf4094194, 0xf5cb2ba3, 0xf78d95fa, 0xf64fffcd,
  0xd9785d60, 0xd8ba3757, 0xdafc890e, 0xdb3ee339,
  0xde71f5bc, 0xdfb39f8b, 0xddf521d2, 0xdc374be5,
  0xd76b0cd8, 0xd6a966ef, 0xd4efd8b6, 0xd52db281,
  0xd062a404, 0xd1a0ce33, 0xd3e6706a, 0xd2241a5d,
  0xc55efe10, 0xc49c9427, 0xc6da2a7e, 0xc7184049,
  0xc25756cc, 0xc3953cfb, 0xc1d382a2, 0xc011e895,
  0xcb4dafa8, 0xca8fc59f, 0xc8c97bc6, 0xc90b11f1,
  0xcc440774, 0xcd866d43, 0xcfc0d31a, 0xce02b92d,
  0x91af9640, 0x906dfc77, 0x922b422e, 0x93e92819,
  0x96a63e9c, 0x976454ab, 0x9522eaf2, 0x94e080c5,
  0x9fbcc7f8, 0x9e7eadcf, 0x9c381396, 0x9dfa79a1,
  0x98b56f24, 0x99770513, 0x9b31bb4a, 0x9af3d17d,
  0x8d893530, 0x8c4b5f07, 0x8e0de15e, 0x8fcf8b69,
  0x8a809dec, 0x8b42f7db, 0x89044982, 0x88c623b5,
  0x839a6488, 0x82580ebf, 0x801eb0e6, 0x81dcdad1,
  0x8493cc54, 0x8551a663, 0x8717183a, 0x86d5720d,
  0xa9e2d0a0, 0xa820ba97, 0xaa6604ce, 0xaba46ef9,
  0xaeeb787c, 0xaf29124b, 0xad6fac12, 0xacadc625,
  0xa7f18118, 0xa633eb2f, 0xa4755576, 0xa5b73f41,
  0xa0f829c4, 0xa13a43f3, 0xa37cfdaa, 0xa2be979d,
  0xb5c473d0, 0xb40619e7, 0xb640a7be, 0xb782cd89,
  0xb2cddb0c, 0xb30fb13b, 0xb1490f62, 0xb08b6555,
  0xbbd72268, 0xba15485f, 0xb853f606, 0xb9919c31,
  0xbcde8ab4, 0xbd1ce083, 0xbf5a5eda, 0xbe9834ed,
  0x00000000, 0xb8bc6765, 0xaa09c88b, 0x12b5afee,
  0x8f629757, 0x37def032, 0x256b5fdc, 0x9dd738b9,
  0xc5b428ef, 0x7d084f8a, 0x6fbde064, 0xd7018701,
  0x4ad6bfb8, 0xf26ad8dd, 0xe0df7733, 0x58631056,
  0x5019579f, 0xe8a530fa, 0xfa109f14, 0x42acf871,
  0xdf7bc0c8, 0x67c7a7ad, 0x75720843, 0xcdce6f26,
  0x95ad7f70, 0x2d111815, 0x3fa4b7fb, 0x8718d09e,
  0x1acfe827, 0xa2738f42, 0xb0c620ac, 0x087a47c9,
  0xa032af3e, 0x188ec85b, 0x0a3b67b5, 0xb28700d0,
  0x2f503869, 0x97ec5f0c, 0x8559f0e2, 0x3de59787,
  0x658687d1, 0xdd3ae0b4, 0xcf8f4f5a, 0x7733283f,
  0xeae41086, 0x525877e3, 0x40edd80d, 0xf851bf68,
  0xf02bf8a1, 0x48979fc4, 0x5a22302a, 0xe29e574f,
  0x7f496ff6, 0xc7f50893, 0xd540a77d, 0x6dfcc018,
  0x359fd04e, 0x8d23b72b, 0x9f9618c5, 0x272a7fa0,
  0xbafd4719, 0x0241207c, 0x10f48f92, 0xa848e8f7,
  0x9b14583d, 0x23a83f58, 0x311d90b6, 0x89a1f7d3,
  0x1476cf6a, 0xaccaa80f, 0xbe7f07e1, 0x06c36084,
  0x5ea070d2, 0xe61c17b7, 0xf4a9b859, 0x4c15df3c,
  0xd1c2e785, 0x697e80e0, 0x7bcb2f0e, 0xc377486b,
  0xcb0d0fa2, 0x73b168c7, 0x6104c729, 0xd9b8a04c,
  0x446f98f5, 0xfcd3ff90, 0xee66507e, 0x56da371b,
  0x0eb9274d, 0xb6054028, 0xa4b0efc6, 0x1c0c88a3,
  0x81dbb01a, 0x3967d77f, 0x2bd27891, 0x936e1ff4,
  0x3b26f703, 0x839a9066, 0x912f3f88, 0x299358ed,
  0xb4446054, 0x0cf80731, 0x1e4da8df, 0xa6f1cfba,
  0xfe92dfec, 0x462eb889, 0x549b1767, 0xec277002,
  0x71f048bb, 0xc94c2fde, 0xdbf98030, 0x6345e755,
  0x6b3fa09c, 0xd383c7f9, 0xc1366817, 0x798a0f72,
  0xe45d37cb, 0x5ce150ae, 0x4e54ff40, 0xf6e89825,
  0xae8b8873, 0x1637ef16, 0x048240f8, 0xbc3e279d,
  0x21e91f24, 0x99557841, 0x8be0d7af, 0x335cb0ca,
  0xed59b63b, 0x55e5d15e, 0x47507eb0, 0xffec19d5,
  0x623b216c, 0xda874609, 0xc832e9e7, 0x708e8e82,
  0x28ed9ed4, 0x9051f9b1, 0x82e4565f, 0x3a58313a,
  0xa78f0983, 0x1f336ee6, 0x0d86c108, 0xb53aa66d,
  0xbd40e1a4, 0x05fc86c1, 0x1749292f, 0xaff54e4a,
  0x322276f3, 0x8a9e1196, 0x982bbe78, 0x2097d91d,
  0x78f4c94b, 0xc048ae2e, 0xd2fd01c0, 0x6a4166a5,
  0xf7965e1c, 0x4f2a3979, 0x5d9f9697, 0xe523f1f2,
  0x4d6b1905, 0xf5d77e60, 0xe762d18e, 0x5fdeb6eb,
  0xc2098e52, 0x7ab5e937, 0x680046d9, 0xd0bc21bc,
  0x88df31ea, 0x3063568f, 0x22d6f961, 0x9a6a9e04,
  0x07bda6bd, 0xbf01c1d8, 0xadb46e36, 0x15080953,
  0x1d724e9a, 0xa5ce29ff, 0xb77b8611, 0x0fc7e174,
  0x9210d9cd, 0x2aacbea8, 0x38191146, 0x80a57623,
  0xd8c66675, 0x607a0110, 0x72cfaefe, 0xca73c99b,
  0x57a4f122, 0xef189647, 0xfdad39a9, 0x45115ecc,
  0x764dee06, 0xcef18963, 0xdc44268d, 0x64f841e8,
  0xf92f7951, 0x41931e34, 0x5326b1da, 0xeb9ad6bf,
  0xb3f9c6e9, 0x0b45a18c, 0x19f00e62, 0xa14c6907,
  0x3c9b51be, 0x842736db, 0x96929935, 0x2e2efe50,
  0x2654b999, 0x9ee8defc, 0x8c5d7112, 0x34e11677,
  0xa9362ece, 0x118a49ab, 0x033fe645, 0xbb838120,
  0xe3e09176, 0x5b5cf613, 0x49e959fd, 0xf1553e98,
  0x6c820621, 0xd43e6144, 0xc68bceaa, 0x7e37a9cf,
  0xd67f4138, 0x6ec3265d, 0x7c7689b3, 0xc4caeed6,
  0x591dd66f, 0xe1a1b10a, 0xf3141ee4, 0x4ba87981,
  0x13cb69d7, 0xab770eb2, 0xb9c2a15c, 0x017ec639,
  0x9ca9fe80, 0x241599e5, 0x36a0360b, 0x8e1c516e,
  0x866616a7, 0x3eda71c2, 0x2c6fde2c, 0x94d3b949,
  0x090481f0, 0xb1b8e695, 0xa30d497b, 0x1bb12e1e,
  0x43d23e48, 0xfb6e592d, 0xe9dbf6c3, 0x516791a6,
  0xccb0a91f, 0x740cce7a, 0x66b96194, 0xde0506f1
};

/* CRC32 */

static inline u32
crc32_next (u32 crc, byte data)
{
  return (crc >> 8) ^ crc32_table[(crc & 0xff) ^ data];
}

/*
 * Process 4 bytes in one go
 */
static inline u32
crc32_next4 (u32 crc, u32 data)
{
  crc ^= data;
  crc = crc32_table[(crc & 0xff) + 0x300] ^
        crc32_table[((crc >> 8) & 0xff) + 0x200] ^
        crc32_table[((crc >> 16) & 0xff) + 0x100] ^
        crc32_table[(crc >> 24) & 0xff];
  return crc;
}

static void
crc32_init (void *context, unsigned int flags)
{
  CRC_CONTEXT *ctx = (CRC_CONTEXT *) context;
  u32 hwf = _gcry_get_hw_features ();

#ifdef USE_INTEL_PCLMUL
  ctx->use_pclmul = (hwf & HWF_INTEL_SSE4_1) && (hwf & HWF_INTEL_PCLMUL);
#endif
#ifdef USE_ARM_PMULL
  ctx->use_pmull = (hwf & HWF_ARM_NEON) && (hwf & HWF_ARM_PMULL);
#endif
#ifdef USE_PPC_VPMSUM
  ctx->use_vpmsum = !!(hwf & HWF_PPC_ARCH_2_07);
#endif

  (void)flags;
  (void)hwf;

  ctx->CRC = 0 ^ 0xffffffffL;
}

static void
crc32_write (void *context, const void *inbuf_arg, size_t inlen)
{
  CRC_CONTEXT *ctx = (CRC_CONTEXT *) context;
  const byte *inbuf = inbuf_arg;
  u32 crc;

#ifdef USE_INTEL_PCLMUL
  if (ctx->use_pclmul)
    {
      _gcry_crc32_intel_pclmul(&ctx->CRC, inbuf, inlen);
      return;
    }
#endif
#ifdef USE_ARM_PMULL
  if (ctx->use_pmull)
    {
      _gcry_crc32_armv8_ce_pmull(&ctx->CRC, inbuf, inlen);
      return;
    }
#endif
#ifdef USE_PPC_VPMSUM
  if (ctx->use_vpmsum)
    {
      _gcry_crc32_ppc8_vpmsum(&ctx->CRC, inbuf, inlen);
      return;
    }
#endif

  if (!inbuf || !inlen)
    return;

  crc = ctx->CRC;

  while (inlen >= 16)
    {
      inlen -= 16;
      crc = crc32_next4(crc, buf_get_le32(&inbuf[0]));
      crc = crc32_next4(crc, buf_get_le32(&inbuf[4]));
      crc = crc32_next4(crc, buf_get_le32(&inbuf[8]));
      crc = crc32_next4(crc, buf_get_le32(&inbuf[12]));
      inbuf += 16;
    }

  while (inlen >= 4)
    {
      inlen -= 4;
      crc = crc32_next4(crc, buf_get_le32(inbuf));
      inbuf += 4;
    }

  while (inlen--)
    {
      crc = crc32_next(crc, *inbuf++);
    }

  ctx->CRC = crc;
}

static byte *
crc32_read (void *context)
{
  CRC_CONTEXT *ctx = (CRC_CONTEXT *) context;
  return ctx->buf;
}

static void
crc32_final (void *context)
{
  CRC_CONTEXT *ctx = (CRC_CONTEXT *) context;
  ctx->CRC ^= 0xffffffffL;
  buf_put_be32 (ctx->buf, ctx->CRC);
}

/* CRC32 a'la RFC 1510 */
/* CRC of the string "123456789" is 0x2dfd2d88 */

static void
crc32rfc1510_init (void *context, unsigned int flags)
{
  CRC_CONTEXT *ctx = (CRC_CONTEXT *) context;
  u32 hwf = _gcry_get_hw_features ();

#ifdef USE_INTEL_PCLMUL
  ctx->use_pclmul = (hwf & HWF_INTEL_SSE4_1) && (hwf & HWF_INTEL_PCLMUL);
#endif
#ifdef USE_ARM_PMULL
  ctx->use_pmull = (hwf & HWF_ARM_NEON) && (hwf & HWF_ARM_PMULL);
#endif
#ifdef USE_PPC_VPMSUM
  ctx->use_vpmsum = !!(hwf & HWF_PPC_ARCH_2_07);
#endif

  (void)flags;
  (void)hwf;

  ctx->CRC = 0;
}

static void
crc32rfc1510_final (void *context)
{
  CRC_CONTEXT *ctx = (CRC_CONTEXT *) context;
  buf_put_be32(ctx->buf, ctx->CRC);
}

/* CRC24 a'la RFC 2440 */
/*
 * Code generated by universal_crc by Danjel McGougan
 *
 * CRC parameters used:
 *   bits:       24
 *   poly:       0x864cfb
 *   init:       0xb704ce
 *   xor:        0x000000
 *   reverse:    false
 *   non-direct: false
 *
 * CRC of the string "123456789" is 0x21cf02
 */

static const u32 crc24_table[1024] =
{
  0x00000000, 0x00fb4c86, 0x000dd58a, 0x00f6990c,
  0x00e1e693, 0x001aaa15, 0x00ec3319, 0x00177f9f,
  0x003981a1, 0x00c2cd27, 0x0034542b, 0x00cf18ad,
  0x00d86732, 0x00232bb4, 0x00d5b2b8, 0x002efe3e,
  0x00894ec5, 0x00720243, 0x00849b4f, 0x007fd7c9,
  0x0068a856, 0x0093e4d0, 0x00657ddc, 0x009e315a,
  0x00b0cf64, 0x004b83e2, 0x00bd1aee, 0x00465668,
  0x005129f7, 0x00aa6571, 0x005cfc7d, 0x00a7b0fb,
  0x00e9d10c, 0x00129d8a, 0x00e40486, 0x001f4800,
  0x0008379f, 0x00f37b19, 0x0005e215, 0x00feae93,
  0x00d050ad, 0x002b1c2b, 0x00dd8527, 0x0026c9a1,
  0x0031b63e, 0x00cafab8, 0x003c63b4, 0x00c72f32,
  0x00609fc9, 0x009bd34f, 0x006d4a43, 0x009606c5,
  0x0081795a, 0x007a35dc, 0x008cacd0, 0x0077e056,
  0x00591e68, 0x00a252ee, 0x0054cbe2, 0x00af8764,
  0x00b8f8fb, 0x0043b47d, 0x00b52d71, 0x004e61f7,
  0x00d2a319, 0x0029ef9f, 0x00df7693, 0x00243a15,
  0x0033458a, 0x00c8090c, 0x003e9000, 0x00c5dc86,
  0x00eb22b8, 0x00106e3e, 0x00e6f732, 0x001dbbb4,
  0x000ac42b, 0x00f188ad, 0x000711a1, 0x00fc5d27,
  0x005beddc, 0x00a0a15a, 0x00563856, 0x00ad74d0,
  0x00ba0b4f, 0x004147c9, 0x00b7dec5, 0x004c9243,
  0x00626c7d, 0x009920fb, 0x006fb9f7, 0x0094f571,
  0x00838aee, 0x0078c668, 0x008e5f64, 0x007513e2,
  0x003b7215, 0x00c03e93, 0x0036a79f, 0x00cdeb19,
  0x00da9486, 0x0021d800, 0x00d7410c, 0x002c0d8a,
  0x0002f3b4, 0x00f9bf32, 0x000f263e, 0x00f46ab8,
  0x00e31527, 0x001859a1, 0x00eec0ad, 0x00158c2b,
  0x00b23cd0, 0x00497056, 0x00bfe95a, 0x0044a5dc,
  0x0053da43, 0x00a896c5, 0x005e0fc9, 0x00a5434f,
  0x008bbd71, 0x0070f1f7, 0x008668fb, 0x007d247d,
  0x006a5be2, 0x00911764, 0x00678e68, 0x009cc2ee,
  0x00a44733, 0x005f0bb5, 0x00a992b9, 0x0052de3f,
  0x0045a1a0, 0x00beed26, 0x0048742a, 0x00b338ac,
  0x009dc692, 0x00668a14, 0x00901318, 0x006b5f9e,
  0x007c2001, 0x00876c87, 0x0071f58b, 0x008ab90d,
  0x002d09f6, 0x00d64570, 0x0020dc7c, 0x00db90fa,
  0x00ccef65, 0x0037a3e3, 0x00c13aef, 0x003a7669,
  0x00148857, 0x00efc4d1, 0x00195ddd, 0x00e2115b,
  0x00f56ec4, 0x000e2242, 0x00f8bb4e, 0x0003f7c8,
  0x004d963f, 0x00b6dab9, 0x004043b5, 0x00bb0f33,
  0x00ac70ac, 0x00573c2a, 0x00a1a526, 0x005ae9a0,
  0x0074179e, 0x008f5b18, 0x0079c214, 0x00828e92,
  0x0095f10d, 0x006ebd8b, 0x00982487, 0x00636801,
  0x00c4d8fa, 0x003f947c, 0x00c90d70, 0x003241f6,
  0x00253e69, 0x00de72ef, 0x0028ebe3, 0x00d3a765,
  0x00fd595b, 0x000615dd, 0x00f08cd1, 0x000bc057,
  0x001cbfc8, 0x00e7f34e, 0x00116a42, 0x00ea26c4,
  0x0076e42a, 0x008da8ac, 0x007b31a0, 0x00807d26,
  0x009702b9, 0x006c4e3f, 0x009ad733, 0x00619bb5,
  0x004f658b, 0x00b4290d, 0x0042b001, 0x00b9fc87,
  0x00ae8318, 0x0055cf9e, 0x00a35692, 0x00581a14,
  0x00ffaaef, 0x0004e669, 0x00f27f65, 0x000933e3,
  0x001e4c7c, 0x00e500fa, 0x001399f6, 0x00e8d570,
  0x00c62b4e, 0x003d67c8, 0x00cbfec4, 0x0030b242,
  0x0027cddd, 0x00dc815b, 0x002a1857, 0x00d154d1,
  0x009f3526, 0x006479a0, 0x0092e0ac, 0x0069ac2a,
  0x007ed3b5, 0x00859f33, 0x0073063f, 0x00884ab9,
  0x00a6b487, 0x005df801, 0x00ab610d, 0x00502d8b,
  0x00475214, 0x00bc1e92, 0x004a879e, 0x00b1cb18,
  0x00167be3, 0x00ed3765, 0x001bae69, 0x00e0e2ef,
  0x00f79d70, 0x000cd1f6, 0x00fa48fa, 0x0001047c,
  0x002ffa42, 0x00d4b6c4, 0x00222fc8, 0x00d9634e,
  0x00ce1cd1, 0x00355057, 0x00c3c95b, 0x003885dd,
  0x00000000, 0x00488f66, 0x00901ecd, 0x00d891ab,
  0x00db711c, 0x0093fe7a, 0x004b6fd1, 0x0003e0b7,
  0x00b6e338, 0x00fe6c5e, 0x0026fdf5, 0x006e7293,
  0x006d9224, 0x00251d42, 0x00fd8ce9, 0x00b5038f,
  0x006cc771, 0x00244817, 0x00fcd9bc, 0x00b456da,
  0x00b7b66d, 0x00ff390b, 0x0027a8a0, 0x006f27c6,
  0x00da2449, 0x0092ab2f, 0x004a3a84, 0x0002b5e2,
  0x00015555, 0x0049da33, 0x00914b98, 0x00d9c4fe,
  0x00d88ee3, 0x00900185, 0x0048902e, 0x00001f48,
  0x0003ffff, 0x004b7099, 0x0093e132, 0x00db6e54,
  0x006e6ddb, 0x0026e2bd, 0x00fe7316, 0x00b6fc70,
  0x00b51cc7, 0x00fd93a1, 0x0025020a, 0x006d8d6c,
  0x00b44992, 0x00fcc6f4, 0x0024575f, 0x006cd839,
  0x006f388e, 0x0027b7e8, 0x00ff2643, 0x00b7a925,
  0x0002aaaa, 0x004a25cc, 0x0092b467, 0x00da3b01,
  0x00d9dbb6, 0x009154d0, 0x0049c57b, 0x00014a1d,
  0x004b5141, 0x0003de27, 0x00db4f8c, 0x0093c0ea,
  0x0090205d, 0x00d8af3b, 0x00003e90, 0x0048b1f6,
  0x00fdb279, 0x00b53d1f, 0x006dacb4, 0x002523d2,
  0x0026c365, 0x006e4c03, 0x00b6dda8, 0x00fe52ce,
  0x00279630, 0x006f1956, 0x00b788fd, 0x00ff079b,
  0x00fce72c, 0x00b4684a, 0x006cf9e1, 0x00247687,
  0x00917508, 0x00d9fa6e, 0x00016bc5, 0x0049e4a3,
  0x004a0414, 0x00028b72, 0x00da1ad9, 0x009295bf,
  0x0093dfa2, 0x00db50c4, 0x0003c16f, 0x004b4e09,
  0x0048aebe, 0x000021d8, 0x00d8b073, 0x00903f15,
  0x00253c9a, 0x006db3fc, 0x00b52257, 0x00fdad31,
  0x00fe4d86, 0x00b6c2e0, 0x006e534b, 0x0026dc2d,
  0x00ff18d3, 0x00b797b5, 0x006f061e, 0x00278978,
  0x002469cf, 0x006ce6a9, 0x00b47702, 0x00fcf864,
  0x0049fbeb, 0x0001748d, 0x00d9e526, 0x00916a40,
  0x00928af7, 0x00da0591, 0x0002943a, 0x004a1b5c,
  0x0096a282, 0x00de2de4, 0x0006bc4f, 0x004e3329,
  0x004dd39e, 0x00055cf8, 0x00ddcd53, 0x00954235,
  0x002041ba, 0x0068cedc, 0x00b05f77, 0x00f8d011,
  0x00fb30a6, 0x00b3bfc0, 0x006b2e6b, 0x0023a10d,
  0x00fa65f3, 0x00b2ea95, 0x006a7b3e, 0x0022f458,
  0x002114ef, 0x00699b89, 0x00b10a22, 0x00f98544,
  0x004c86cb, 0x000409ad, 0x00dc9806, 0x00941760,
  0x0097f7d7, 0x00df78b1, 0x0007e91a, 0x004f667c,
  0x004e2c61, 0x0006a307, 0x00de32ac, 0x0096bdca,
  0x00955d7d, 0x00ddd21b, 0x000543b0, 0x004dccd6,
  0x00f8cf59, 0x00b0403f, 0x0068d194, 0x00205ef2,
  0x0023be45, 0x006b3123, 0x00b3a088, 0x00fb2fee,
  0x0022eb10, 0x006a6476, 0x00b2f5dd, 0x00fa7abb,
  0x00f99a0c, 0x00b1156a, 0x006984c1, 0x00210ba7,
  0x00940828, 0x00dc874e, 0x000416e5, 0x004c9983,
  0x004f7934, 0x0007f652, 0x00df67f9, 0x0097e89f,
  0x00ddf3c3, 0x00957ca5, 0x004ded0e, 0x00056268,
  0x000682df, 0x004e0db9, 0x00969c12, 0x00de1374,
  0x006b10fb, 0x00239f9d, 0x00fb0e36, 0x00b38150,
  0x00b061e7, 0x00f8ee81, 0x00207f2a, 0x0068f04c,
  0x00b134b2, 0x00f9bbd4, 0x00212a7f, 0x0069a519,
  0x006a45ae, 0x0022cac8, 0x00fa5b63, 0x00b2d405,
  0x0007d78a, 0x004f58ec, 0x0097c947, 0x00df4621,
  0x00dca696, 0x009429f0, 0x004cb85b, 0x0004373d,
  0x00057d20, 0x004df246, 0x009563ed, 0x00ddec8b,
  0x00de0c3c, 0x0096835a, 0x004e12f1, 0x00069d97,
  0x00b39e18, 0x00fb117e, 0x002380d5, 0x006b0fb3,
  0x0068ef04, 0x00206062, 0x00f8f1c9, 0x00b07eaf,
  0x0069ba51, 0x00213537, 0x00f9a49c, 0x00b12bfa,
  0x00b2cb4d, 0x00fa442b, 0x0022d580, 0x006a5ae6,
  0x00df5969, 0x0097d60f, 0x004f47a4, 0x0007c8c2,
  0x00042875, 0x004ca713, 0x009436b8, 0x00dcb9de,
  0x00000000, 0x00d70983, 0x00555f80, 0x00825603,
  0x0051f286, 0x0086fb05, 0x0004ad06, 0x00d3a485,
  0x0059a88b, 0x008ea108, 0x000cf70b, 0x00dbfe88,
  0x00085a0d, 0x00df538e, 0x005d058d, 0x008a0c0e,
  0x00491c91, 0x009e1512, 0x001c4311, 0x00cb4a92,
  0x0018ee17, 0x00cfe794, 0x004db197, 0x009ab814,
  0x0010b41a, 0x00c7bd99, 0x0045eb9a, 0x0092e219,
  0x0041469c, 0x00964f1f, 0x0014191c, 0x00c3109f,
  0x006974a4, 0x00be7d27, 0x003c2b24, 0x00eb22a7,
  0x00388622, 0x00ef8fa1, 0x006dd9a2, 0x00bad021,
  0x0030dc2f, 0x00e7d5ac, 0x006583af, 0x00b28a2c,
  0x00612ea9, 0x00b6272a, 0x00347129, 0x00e378aa,
  0x00206835, 0x00f761b6, 0x007537b5, 0x00a23e36,
  0x00719ab3, 0x00a69330, 0x0024c533, 0x00f3ccb0,
  0x0079c0be, 0x00aec93d, 0x002c9f3e, 0x00fb96bd,
  0x00283238, 0x00ff3bbb, 0x007d6db8, 0x00aa643b,
  0x0029a4ce, 0x00fead4d, 0x007cfb4e, 0x00abf2cd,
  0x00785648, 0x00af5fcb, 0x002d09c8, 0x00fa004b,
  0x00700c45, 0x00a705c6, 0x002553c5, 0x00f25a46,
  0x0021fec3, 0x00f6f740, 0x0074a143, 0x00a3a8c0,
  0x0060b85f, 0x00b7b1dc, 0x0035e7df, 0x00e2ee5c,
  0x00314ad9, 0x00e6435a, 0x00641559, 0x00b31cda,
  0x003910d4, 0x00ee1957, 0x006c4f54, 0x00bb46d7,
  0x0068e252, 0x00bfebd1, 0x003dbdd2, 0x00eab451,
  0x0040d06a, 0x0097d9e9, 0x00158fea, 0x00c28669,
  0x001122ec, 0x00c62b6f, 0x00447d6c, 0x009374ef,
  0x001978e1, 0x00ce7162, 0x004c2761, 0x009b2ee2,
  0x00488a67, 0x009f83e4, 0x001dd5e7, 0x00cadc64,
  0x0009ccfb, 0x00dec578, 0x005c937b, 0x008b9af8,
  0x00583e7d, 0x008f37fe, 0x000d61fd, 0x00da687e,
  0x00506470, 0x00876df3, 0x00053bf0, 0x00d23273,
  0x000196f6, 0x00d69f75, 0x0054c976, 0x0083c0f5,
  0x00a9041b, 0x007e0d98, 0x00fc5b9b, 0x002b5218,
  0x00f8f69d, 0x002fff1e, 0x00ada91d, 0x007aa09e,
  0x00f0ac90, 0x0027a513, 0x00a5f310, 0x0072fa93,
  0x00a15e16, 0x00765795, 0x00f40196, 0x00230815,
  0x00e0188a, 0x00371109, 0x00b5470a, 0x00624e89,
  0x00b1ea0c, 0x0066e38f, 0x00e4b58c, 0x0033bc0f,
  0x00b9b001, 0x006eb982, 0x00ecef81, 0x003be602,
  0x00e84287, 0x003f4b04, 0x00bd1d07, 0x006a1484,
  0x00c070bf, 0x0017793c, 0x00952f3f, 0x004226bc,
  0x00918239, 0x00468bba, 0x00c4ddb9, 0x0013d43a,
  0x0099d834, 0x004ed1b7, 0x00cc87b4, 0x001b8e37,
  0x00c82ab2, 0x001f2331, 0x009d7532, 0x004a7cb1,
  0x00896c2e, 0x005e65ad, 0x00dc33ae, 0x000b3a2d,
  0x00d89ea8, 0x000f972b, 0x008dc128, 0x005ac8ab,
  0x00d0c4a5, 0x0007cd26, 0x00859b25, 0x005292a6,
  0x00813623, 0x00563fa0, 0x00d469a3, 0x00036020,
  0x0080a0d5, 0x0057a956, 0x00d5ff55, 0x0002f6d6,
  0x00d15253, 0x00065bd0, 0x00840dd3, 0x00530450,
  0x00d9085e, 0x000e01dd, 0x008c57de, 0x005b5e5d,
  0x0088fad8, 0x005ff35b, 0x00dda558, 0x000aacdb,
  0x00c9bc44, 0x001eb5c7, 0x009ce3c4, 0x004bea47,
  0x00984ec2, 0x004f4741, 0x00cd1142, 0x001a18c1,
  0x009014cf, 0x00471d4c, 0x00c54b4f, 0x001242cc,
  0x00c1e649, 0x0016efca, 0x0094b9c9, 0x0043b04a,
  0x00e9d471, 0x003eddf2, 0x00bc8bf1, 0x006b8272,
  0x00b826f7, 0x006f2f74, 0x00ed7977, 0x003a70f4,
  0x00b07cfa, 0x00677579, 0x00e5237a, 0x00322af9,
  0x00e18e7c, 0x003687ff, 0x00b4d1fc, 0x0063d87f,
  0x00a0c8e0, 0x0077c163, 0x00f59760, 0x00229ee3,
  0x00f13a66, 0x002633e5, 0x00a465e6, 0x00736c65,
  0x00f9606b, 0x002e69e8, 0x00ac3feb, 0x007b3668,
  0x00a892ed, 0x007f9b6e, 0x00fdcd6d, 0x002ac4ee,
  0x00000000, 0x00520936, 0x00a4126c, 0x00f61b5a,
  0x004825d8, 0x001a2cee, 0x00ec37b4, 0x00be3e82,
  0x006b0636, 0x00390f00, 0x00cf145a, 0x009d1d6c,
  0x002323ee, 0x00712ad8, 0x00873182, 0x00d538b4,
  0x00d60c6c, 0x0084055a, 0x00721e00, 0x00201736,
  0x009e29b4, 0x00cc2082, 0x003a3bd8, 0x006832ee,
  0x00bd0a5a, 0x00ef036c, 0x00191836, 0x004b1100,
  0x00f52f82, 0x00a726b4, 0x00513dee, 0x000334d8,
  0x00ac19d8, 0x00fe10ee, 0x00080bb4, 0x005a0282,
  0x00e43c00, 0x00b63536, 0x00402e6c, 0x0012275a,
  0x00c71fee, 0x009516d8, 0x00630d82, 0x003104b4,
  0x008f3a36, 0x00dd3300, 0x002b285a, 0x0079216c,
  0x007a15b4, 0x00281c82, 0x00de07d8, 0x008c0eee,
  0x0032306c, 0x0060395a, 0x00962200, 0x00c42b36,
  0x00111382, 0x00431ab4, 0x00b501ee, 0x00e708d8,
  0x0059365a, 0x000b3f6c, 0x00fd2436, 0x00af2d00,
  0x00a37f36, 0x00f17600, 0x00076d5a, 0x0055646c,
  0x00eb5aee, 0x00b953d8, 0x004f4882, 0x001d41b4,
  0x00c87900, 0x009a7036, 0x006c6b6c, 0x003e625a,
  0x00805cd8, 0x00d255ee, 0x00244eb4, 0x00764782,
  0x0075735a, 0x00277a6c, 0x00d16136, 0x00836800,
  0x003d5682, 0x006f5fb4, 0x009944ee, 0x00cb4dd8,
  0x001e756c, 0x004c7c5a, 0x00ba6700, 0x00e86e36,
  0x005650b4, 0x00045982, 0x00f242d8, 0x00a04bee,
  0x000f66ee, 0x005d6fd8, 0x00ab7482, 0x00f97db4,
  0x00474336, 0x00154a00, 0x00e3515a, 0x00b1586c,
  0x006460d8, 0x003669ee, 0x00c072b4, 0x00927b82,
  0x002c4500, 0x007e4c36, 0x0088576c, 0x00da5e5a,
  0x00d96a82, 0x008b63b4, 0x007d78ee, 0x002f71d8,
  0x00914f5a, 0x00c3466c, 0x00355d36, 0x00675400,
  0x00b26cb4, 0x00e06582, 0x00167ed8, 0x004477ee,
  0x00fa496c, 0x00a8405a, 0x005e5b00, 0x000c5236,
  0x0046ff6c, 0x0014f65a, 0x00e2ed00, 0x00b0e436,
  0x000edab4, 0x005cd382, 0x00aac8d8, 0x00f8c1ee,
  0x002df95a, 0x007ff06c, 0x0089eb36, 0x00dbe200,
  0x0065dc82, 0x0037d5b4, 0x00c1ceee, 0x0093c7d8,
  0x0090f300, 0x00c2fa36, 0x0034e16c, 0x0066e85a,
  0x00d8d6d8, 0x008adfee, 0x007cc4b4, 0x002ecd82,
  0x00fbf536, 0x00a9fc00, 0x005fe75a, 0x000dee6c,
  0x00b3d0ee, 0x00e1d9d8, 0x0017c282, 0x0045cbb4,
  0x00eae6b4, 0x00b8ef82, 0x004ef4d8, 0x001cfdee,
  0x00a2c36c, 0x00f0ca5a, 0x0006d100, 0x0054d836,
  0x0081e082, 0x00d3e9b4, 0x0025f2ee, 0x0077fbd8,
  0x00c9c55a, 0x009bcc6c, 0x006dd736, 0x003fde00,
  0x003cead8, 0x006ee3ee, 0x0098f8b4, 0x00caf182,
  0x0074cf00, 0x0026c636, 0x00d0dd6c, 0x0082d45a,
  0x0057ecee, 0x0005e5d8, 0x00f3fe82, 0x00a1f7b4,
  0x001fc936, 0x004dc000, 0x00bbdb5a, 0x00e9d26c,
  0x00e5805a, 0x00b7896c, 0x00419236, 0x00139b00,
  0x00ada582, 0x00ffacb4, 0x0009b7ee, 0x005bbed8,
  0x008e866c, 0x00dc8f5a, 0x002a9400, 0x00789d36,
  0x00c6a3b4, 0x0094aa82, 0x0062b1d8, 0x0030b8ee,
  0x00338c36, 0x00618500, 0x00979e5a, 0x00c5976c,
  0x007ba9ee, 0x0029a0d8, 0x00dfbb82, 0x008db2b4,
  0x00588a00, 0x000a8336, 0x00fc986c, 0x00ae915a,
  0x0010afd8, 0x0042a6ee, 0x00b4bdb4, 0x00e6b482,
  0x00499982, 0x001b90b4, 0x00ed8bee, 0x00bf82d8,
  0x0001bc5a, 0x0053b56c, 0x00a5ae36, 0x00f7a700,
  0x00229fb4, 0x00709682, 0x00868dd8, 0x00d484ee,
  0x006aba6c, 0x0038b35a, 0x00cea800, 0x009ca136,
  0x009f95ee, 0x00cd9cd8, 0x003b8782, 0x00698eb4,
  0x00d7b036, 0x0085b900, 0x0073a25a, 0x0021ab6c,
  0x00f493d8, 0x00a69aee, 0x005081b4, 0x00028882,
  0x00bcb600, 0x00eebf36, 0x0018a46c, 0x004aad5a
};

static inline
u32 crc24_init (void)
{
  /* Transformed to 32-bit CRC by multiplied by x⁸ and then byte swapped. */
  return 0xce04b7; /* _gcry_bswap(0xb704ce << 8) */
}

static inline
u32 crc24_next (u32 crc, byte data)
{
  return (crc >> 8) ^ crc24_table[(crc & 0xff) ^ data];
}

/*
 * Process 4 bytes in one go
 */
static inline
u32 crc24_next4 (u32 crc, u32 data)
{
  crc ^= data;
  crc = crc24_table[(crc & 0xff) + 0x300] ^
        crc24_table[((crc >> 8) & 0xff) + 0x200] ^
        crc24_table[((crc >> 16) & 0xff) + 0x100] ^
        crc24_table[(data >> 24) & 0xff];
  return crc;
}

static inline
u32 crc24_final (u32 crc)
{
  return crc & 0xffffff;
}

static void
crc24rfc2440_init (void *context, unsigned int flags)
{
  CRC_CONTEXT *ctx = (CRC_CONTEXT *) context;
  u32 hwf = _gcry_get_hw_features ();

#ifdef USE_INTEL_PCLMUL
  ctx->use_pclmul = (hwf & HWF_INTEL_SSE4_1) && (hwf & HWF_INTEL_PCLMUL);
#endif
#ifdef USE_ARM_PMULL
  ctx->use_pmull = (hwf & HWF_ARM_NEON) && (hwf & HWF_ARM_PMULL);
#endif
#ifdef USE_PPC_VPMSUM
  ctx->use_vpmsum = !!(hwf & HWF_PPC_ARCH_2_07);
#endif

  (void)hwf;
  (void)flags;

  ctx->CRC = crc24_init();
}

static void
crc24rfc2440_write (void *context, const void *inbuf_arg, size_t inlen)
{
  const unsigned char *inbuf = inbuf_arg;
  CRC_CONTEXT *ctx = (CRC_CONTEXT *) context;
  u32 crc;

#ifdef USE_INTEL_PCLMUL
  if (ctx->use_pclmul)
    {
      _gcry_crc24rfc2440_intel_pclmul(&ctx->CRC, inbuf, inlen);
      return;
    }
#endif
#ifdef USE_ARM_PMULL
  if (ctx->use_pmull)
    {
      _gcry_crc24rfc2440_armv8_ce_pmull(&ctx->CRC, inbuf, inlen);
      return;
    }
#endif
#ifdef USE_PPC_VPMSUM
  if (ctx->use_vpmsum)
    {
      _gcry_crc24rfc2440_ppc8_vpmsum(&ctx->CRC, inbuf, inlen);
      return;
    }
#endif

  if (!inbuf || !inlen)
    return;

  crc = ctx->CRC;

  while (inlen >= 16)
    {
      inlen -= 16;
      crc = crc24_next4(crc, buf_get_le32(&inbuf[0]));
      crc = crc24_next4(crc, buf_get_le32(&inbuf[4]));
      crc = crc24_next4(crc, buf_get_le32(&inbuf[8]));
      crc = crc24_next4(crc, buf_get_le32(&inbuf[12]));
      inbuf += 16;
    }

  while (inlen >= 4)
    {
      inlen -= 4;
      crc = crc24_next4(crc, buf_get_le32(inbuf));
      inbuf += 4;
    }

  while (inlen--)
    {
      crc = crc24_next(crc, *inbuf++);
    }

  ctx->CRC = crc;
}

static void
crc24rfc2440_final (void *context)
{
  CRC_CONTEXT *ctx = (CRC_CONTEXT *) context;
  ctx->CRC = crc24_final(ctx->CRC);
  buf_put_le32 (ctx->buf, ctx->CRC);
}

/* We allow the CRC algorithms even in FIPS mode because they are
   actually no cryptographic primitives.  */

gcry_md_spec_t _gcry_digest_spec_crc32 =
  {
    GCRY_MD_CRC32, {0, 1},
    "CRC32", NULL, 0, NULL, 4,
    crc32_init, crc32_write, crc32_final, crc32_read, NULL,
    NULL,
    sizeof (CRC_CONTEXT)
  };

gcry_md_spec_t _gcry_digest_spec_crc32_rfc1510 =
  {
    GCRY_MD_CRC32_RFC1510, {0, 1},
    "CRC32RFC1510", NULL, 0, NULL, 4,
    crc32rfc1510_init, crc32_write, crc32rfc1510_final, crc32_read, NULL,
    NULL,
    sizeof (CRC_CONTEXT)
  };

gcry_md_spec_t _gcry_digest_spec_crc24_rfc2440 =
  {
    GCRY_MD_CRC24_RFC2440, {0, 1},
    "CRC24RFC2440", NULL, 0, NULL, 3,
    crc24rfc2440_init, crc24rfc2440_write, crc24rfc2440_final, crc32_read, NULL,
    NULL,
    sizeof (CRC_CONTEXT)
  };
