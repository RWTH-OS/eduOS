/*
 * Copyright (c) 2010, Stefan Lankes
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    * Neither the name of the <organization> nor the
 *      names of its contributors may be used to endorse or promote products
 *      derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <eduos/string.h>

#ifndef HAVE_ARCH_MEMCPY
void *memcpy(void *dest, const void *src, size_t count)
{
	size_t i;

	if (BUILTIN_EXPECT(!dest || !src, 0))
		return dest;

	for (i = 0; i < count; i++)
		((char*)dest)[i] = ((char*)src)[i];
	
	return dest;
}
#endif

#ifndef HAVE_ARCH_MEMSET
void *memset(void *dest, int val, size_t count)
{
	size_t i;

	if (BUILTIN_EXPECT(!dest, 0))
		return dest;

	for (i = 0; i < count; i++)
		((char*) dest)[i] = (char) val;

	return dest;
}
#endif

#ifndef HAVE_ARCH_STRLEN
size_t strlen(const char *str)
{
	size_t len = 0;

	if (BUILTIN_EXPECT(!str, 0))
		return len;

	while (str[len] != '\0')
		len++;

	return len;
}
#endif

#ifndef HAVE_ARCH_STRNCPY
char* strncpy(char *dest, const char *src, size_t n)
{
	size_t i;

	if (BUILTIN_EXPECT(!dest || !src, 0))
		return dest;

	for (i = 0 ; i < n && src[i] != '\0' ; i++)
		dest[i] = src[i];
	if (i < n)
		dest[i] = '\0';
	else
		dest[n-1] = '\0';

	return dest;
}
#endif

#ifndef HAVE_ARCH_STRCPY
char* strcpy(char *dest, const char *src)
{
        size_t i;

	if (BUILTIN_EXPECT(!dest || !src, 0))
		return dest;

        for (i = 0 ; src[i] != '\0' ; i++)
                dest[i] = src[i];
        dest[i] = '\0';

        return dest;
}
#endif

#ifndef HAVE_ARCH_STRCMP
int strcmp(const char *s1, const char *s2)
{
	while (*s1 != '\0' && *s1 == *s2) {
		s1++;
		s2++;
	}

	return (*(unsigned char *) s1) - (*(unsigned char *) s2);
}
#endif

#ifndef HAVE_ARCH_STRNCMP
int strncmp(const char *s1, const char *s2, size_t n)
{
	if (BUILTIN_EXPECT(n == 0, 0))
		return 0;

	while (n-- != 0 && *s1 == *s2) {
		if (n == 0 || *s1 == '\0')
			break;
		s1++;
		s2++;
	}

	return (*(unsigned char *) s1) - (*(unsigned char *) s2);
}
#endif
