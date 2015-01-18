/*
 * Copyright (c) 2010, Stefan Lankes, RWTH Aachen University
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    * Neither the name of the University nor the names of its contributors
 *      may be used to endorse or promote products derived from this
 *      software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>

/*file descriptor init*/
#define NR_OPEN 10
#define FS_INIT { [0 ... NR_OPEN-1] = {NULL, 0, 0} }

#define TESTSTR "hello in new file '/bin/test.txt'"

int main(int argc, char** argv)
{
	int i, testfile;
	char* teststr;
	DIR* testdir;
	dirent_t* testdirent;

	for(i=0; environ[i]; i++)
		printf("environ[%d] = %s\n", i, environ[i]);
	for(i=0; i<argc; i++)
		printf("argv[%d] = %s\n", i, argv[i]);

	teststr = malloc(sizeof(char)*100);
	if (!teststr)
		exit(1);

	testfile = open("/bin/test.txt", O_CREAT | O_EXCL, "wr");
	if (testfile < 1) {
		printf("error: Was not able to open /bin/test.txt\n");
		exit(1);
	}
	write(testfile, TESTSTR, 34);
	lseek(testfile, 0, SEEK_SET);
	read(testfile, teststr, 100);
	close(testfile);

	printf("read from new file: %s\n", teststr);

	if (strcmp(teststr, TESTSTR))
		exit(1);

	testdir = opendir("/bin/");
	if (testdir < 1) {
		printf("error: Was not able to open /bin directory");
		exit(1);
	}
	testdirent = readdir(testdir);
	printf("1. Dirent: %s\n", testdirent->d_name);
	closedir(testdir);

	return errno;
}
