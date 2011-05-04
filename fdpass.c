/*
   +----------------------------------------------------------------------+
   | All rights reserved                                                  |
   |                                                                      |
   | Redistribution and use in source and binary forms, with or without   |
   | modification, are permitted provided that the following conditions   |
   | are met:                                                             |
   |                                                                      |
   | 1. Redistributions of source code must retain the above copyright    |
   |    notice, this list of conditions and the following disclaimer.     |
   | 2. Redistributions in binary form must reproduce the above copyright |
   |    notice, this list of conditions and the following disclaimer in   |
   |    the documentation and/or other materials provided with the        |
   |    distribution.                                                     |
   | 3. The names of the authors may not be used to endorse or promote    |
   |    products derived from this software without specific prior        |
   |    written permission.                                               |
   |                                                                      |
   | THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS  |
   | "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT    |
   | LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS    |
   | FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE       |
   | COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,  |
   | INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, |
   | BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;     |
   | LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER     |
   | CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT   |
   | LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN    | 
   | ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE      |
   | POSSIBILITY OF SUCH DAMAGE.                                          |
   +----------------------------------------------------------------------+
   | Authors: Jille Timmermans <jille@quis.cx>                            |
   +----------------------------------------------------------------------+
*/

/* $ Id: $ */ 

#include "zend.h"
#include "php.h"
#include "php_network.h"
#include "php_fdpass.h"

#if HAVE_FDPASS

/* {{{ fdpass_functions[] */
function_entry fdpass_functions[] = {
	PHP_FE(fdpass_send         , fdpass_send_arg_info)
	PHP_FE(fdpass_recv         , fdpass_recv_arg_info)
	{ NULL, NULL, NULL }
};
/* }}} */


/* {{{ fdpass_module_entry
 */
zend_module_entry fdpass_module_entry = {
	STANDARD_MODULE_HEADER,
	"fdpass",
	fdpass_functions,
	NULL,
	NULL,
	NULL,
	NULL,
	PHP_MINFO(fdpass),
	PHP_FDPASS_VERSION, 
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_FDPASS
ZEND_GET_MODULE(fdpass)
#endif

/* {{{ PHP_MINFO_FUNCTION */
PHP_MINFO_FUNCTION(fdpass)
{
	php_printf("An extension which provides filedescriptor passing\n");
	php_info_print_table_start();
	php_info_print_table_row(2, "Version",PHP_FDPASS_VERSION " (alpha)");
	php_info_print_table_row(2, "Released", "2011-04-24");
	// php_info_print_table_row(2, "CVS Revision", "$Id: $");
	php_info_print_table_row(2, "Authors", "Jille Timmermans 'jille@quis.cx' (lead)\n");
	php_info_print_table_end();
}
/* }}} */


/* {{{ proto int fdpass_send(stream localfd, stream transferfd, string data)
  Send transferfd over to the remote side of localfd */
PHP_FUNCTION(fdpass_send)
{
	zval *localfd_zval = NULL;
	php_stream *localfd_stream = NULL;
	zval *transferfd_zval = NULL;
	php_stream *transferfd_stream = NULL;
	const char *data = NULL;
	int data_len = 0;

	int ret, localfd, transferfd;
	struct msghdr hdr;
	struct iovec iov_data;
	struct cmsghdr *cmsg;
	char cmsg_buf[CMSG_SPACE(sizeof(int))];

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rrs", &localfd_zval, &transferfd_zval, &data, &data_len) == FAILURE) {
		return;
	}
	php_stream_from_zval(localfd_stream, &localfd_zval);
	php_stream_from_zval(transferfd_stream, &transferfd_zval);

	if(strcmp(localfd_stream->ops->label, "generic_socket") != 0 && strcmp(localfd_stream->ops->label, "tcp_socket") != 0 && strcmp(localfd_stream->ops->label, "udp_socket") != 0 && strcmp(localfd_stream->ops->label, "unix_socket") != 0 && strcmp(localfd_stream->ops->label, "udg_socket") != 0) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "socket type '%s' not supported", localfd_stream->ops->label);
		RETURN_FALSE;
	}

/* XXX Dit kan netter; zoals libevent:
	if (ZEND_FETCH_RESOURCE_NO_RETURN(stream, php_stream *, fd, -1, NULL, php_file_le_stream())) {
		if (php_stream_cast(stream, PHP_STREAM_AS_FD_FOR_SELECT | PHP_STREAM_CAST_INTERNAL, (void*)&file_desc, 1) != SUCCESS || file_desc < 0) {
			RETURN_FALSE;
		}
	}
 */
	transferfd = ((php_netstream_data_t *)transferfd_stream->abstract)->socket;
	localfd = ((php_netstream_data_t *)localfd_stream->abstract)->socket;
	printf("fd: pass %d to %d\n", transferfd, localfd);

	memset(&hdr, 0, sizeof(hdr));
	memset(&iov_data, 0, sizeof(iov_data));
	memset(cmsg_buf, 0, sizeof(cmsg_buf));

	iov_data.iov_base = (char *)data;
	iov_data.iov_len = data_len;
	hdr.msg_name = NULL;
	hdr.msg_namelen = 0;
	hdr.msg_iov = &iov_data;
	hdr.msg_iovlen = 1;
	hdr.msg_flags = 0;
	hdr.msg_control = cmsg_buf;
	hdr.msg_controllen = CMSG_LEN(sizeof(int));
	cmsg = CMSG_FIRSTHDR(&hdr);
	cmsg->cmsg_len = hdr.msg_controllen;
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	((int *)CMSG_DATA(cmsg))[0] = transferfd;

	ret = sendmsg(localfd, &hdr, 0);

	if(ret == -1) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "sendmsg() failed: %s", strerror(errno));
		RETURN_FALSE;
	}

	RETVAL_LONG(ret);
}
/* }}} fdpass_send */

/* {{{ proto stream fdpass_recv(stream localfd, stream &stream [, int len = 512 ])
  Receives the filedescriptor passed by fdpass_send() */
PHP_FUNCTION(fdpass_recv)
{
	zval *localfd_zval = NULL;
	zval *outstream_zval = NULL;
	php_stream *localfd_stream = NULL;
	long len = 512;

	int localfd, ret;
	struct msghdr hdr;
	struct iovec iov_data;
	struct cmsghdr *cmsg;
	char cmsg_buf[CMSG_SPACE(sizeof(int))];

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rz|l", &localfd_zval, &outstream_zval, &len) == FAILURE) {
		return;
	}
	php_stream_from_zval(localfd_stream, &localfd_zval);

	if(strcmp(localfd_stream->ops->label, "generic_socket") != 0 && strcmp(localfd_stream->ops->label, "tcp_socket") != 0 && strcmp(localfd_stream->ops->label, "udp_socket") != 0 && strcmp(localfd_stream->ops->label, "unix_socket") != 0 && strcmp(localfd_stream->ops->label, "udg_socket") != 0) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "socket type '%s' not supported", localfd_stream->ops->label);
		RETURN_FALSE;
	}

/* XXX Dit kan netter; zie fdpass_send */
	localfd = ((php_netstream_data_t *)localfd_stream->abstract)->socket;

	memset(&hdr, 0, sizeof(hdr));
	memset(&iov_data, 0, sizeof(iov_data));
	memset(cmsg_buf, 0, sizeof(cmsg_buf));

	iov_data.iov_base = emalloc(len);
	iov_data.iov_len = len;
	hdr.msg_name = NULL;
	hdr.msg_namelen = 0;
	hdr.msg_iov = &iov_data;
	hdr.msg_iovlen = 1;
	hdr.msg_flags = 0;
	hdr.msg_control = cmsg_buf;
	hdr.msg_controllen = sizeof(cmsg_buf);

	ret = recvmsg(localfd, &hdr, 0);

	if(ret == -1) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "recvmsg() failed: %s", strerror(errno));
		efree(iov_data.iov_base);
		RETURN_FALSE;
	}

	cmsg = CMSG_FIRSTHDR(&hdr);
	if(cmsg->cmsg_type == SCM_RIGHTS) {
		int *fdp = (int *)CMSG_DATA(cmsg);
		int nfds = ((caddr_t)cmsg + cmsg->cmsg_len - (caddr_t)fdp) / sizeof(int);

		if(nfds > 0) {
			int fd = ((int *)CMSG_DATA(cmsg))[0];
			if(nfds > 1) {
				// TODO: Close the other descriptors
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Received %d descriptors instead of 1", nfds);
			}
			if(fd >= 0) {
				php_stream *outstream_stream = php_stream_sock_open_from_socket(fd, 0);
				php_stream_to_zval(outstream_stream, outstream_zval);
			} else {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Received invalid filedescriptor");
			}
		}
	} else {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Received control message of unknown type");
	}

	RETURN_STRINGL(iov_data.iov_base, ret, 0);
}
/* }}} fdpass_recv */

#endif /* HAVE_FDPASS */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
