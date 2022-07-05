#ifndef _LL_FILE_H_
#define _LL_FILE_H_

/**
 * @file
 *
 * LL Logs API
 *
 * This file provides a log API to RTE applications.
 */
#ifdef __cplusplus
extern "C" {
#endif

/**
 * check file exists
 *
 * @param file_name
 *  The file you want to check.
 * @return
 *   - 1: exists.
 *   - 0: not exists.
 */
int
ll_file_exists(const char *file_name);



/**
 * read data from fd
 *
 * @param fd
 *  The file description.
 * 
 * @param buf
 *  The output buffer
 * 
 * @param file_sz
 *  The size want to read
 * 
 * @return
 *   - -1: something wrong.
 *   - >= 0: the data has been read.
 */
ssize_t
ll_file_read_from_fd(int fd, uint8_t *buf, size_t file_sz);


/**
 * read data from file
 *
 * @param file_name
 *  The file name.
 * 
 * @param buf
 *  The output buffer.
 * 
 * @param file_max_sz
 *  The file max size.
 * 
 * @return
 *   - -1: something wrong.
 *   - >= 0: the data has been read.
 */
ssize_t
ll_file_read_from_file(const char* file_name, uint8_t* buf, size_t file_max_sz);

#ifdef __cplusplus
}
#endif

#endif //_LL_FILE_H_