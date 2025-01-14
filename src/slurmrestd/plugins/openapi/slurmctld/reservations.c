/*****************************************************************************\
 *  reservations.c - Slurm REST API reservations http operations handlers
 *****************************************************************************
 *  Copyright (C) 2020 UT-Battelle, LLC.
 *  Written by Matt Ezell <ezellma@ornl.gov>
 *
 *  This file is part of Slurm, a resource management program.
 *  For details, see <https://slurm.schedmd.com/>.
 *  Please also read the included file: DISCLAIMER.
 *
 *  Slurm is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  In addition, as a special exception, the copyright holders give permission
 *  to link the code of portions of this program with the OpenSSL library under
 *  certain conditions as described in each individual source file, and
 *  distribute linked combinations including the two. You must obey the GNU
 *  General Public License in all respects for all of the code used other than
 *  OpenSSL. If you modify file(s) with this exception, you may extend this
 *  exception to your version of the file(s), but you are not obligated to do
 *  so. If you do not wish to do so, delete this exception statement from your
 *  version.  If you delete this exception statement from all source files in
 *  the program, then also delete it here.
 *
 *  Slurm is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with Slurm; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA.
\*****************************************************************************/

#include "src/common/xassert.h"
#include "src/common/xmalloc.h"
#include "src/common/xstring.h"

#include "src/interfaces/data_parser.h"

#include "src/slurmrestd/operations.h"

#include "api.h"

extern int _op_handler_reservations(const char *context_id,
				    http_request_method_t method,
				    data_t *parameters, data_t *query, int tag,
				    data_t *resp, void *auth)
{
	int rc;
	reserve_info_msg_t *res_info_ptr = NULL;
	time_t update_time = 0;
	ctxt_t *ctxt = init_connection(context_id, method, parameters, query,
				       tag, resp, auth);

	if (ctxt->rc)
		goto done;

	if (method != HTTP_REQUEST_GET) {
		resp_error(ctxt, ESLURM_REST_INVALID_QUERY, __func__,
			   "Unsupported HTTP method requested: %s",
			   get_http_method_string(method));
		goto done;
	}

	if ((rc = get_date_param(query, "update_time", &update_time)))
		goto done;

	errno = 0;
	if ((rc = slurm_load_reservations(update_time, &res_info_ptr))) {
		if (rc == SLURM_ERROR)
			rc = errno;

		resp_error(ctxt, rc, "slurm_load_reservations()",
			   "Unable to query reservations");

		goto done;
	}

	DUMP_OPENAPI_RESP_SINGLE(OPENAPI_RESERVATION_RESP, res_info_ptr, ctxt);

done:
	slurm_free_reservation_info_msg(res_info_ptr);
	return fini_connection(ctxt);
}

extern int _op_handler_reservation(const char *context_id,
				   http_request_method_t method,
				   data_t *parameters, data_t *query, int tag,
				   data_t *resp, void *auth)
{
	int rc;
	reserve_info_msg_t *res_info_ptr = NULL;
	reserve_info_t *res = NULL;
	time_t update_time = 0;
	char *name = NULL;
	ctxt_t *ctxt = init_connection(context_id, method, parameters, query,
				       tag, resp, auth);

	if (ctxt->rc)
		goto done;

	if (method != HTTP_REQUEST_GET) {
		resp_error(ctxt, ESLURM_REST_INVALID_QUERY, __func__,
			   "Unsupported HTTP method requested: %s",
			   get_http_method_string(method));
		goto done;
	}

	if ((rc = get_date_param(query, "update_time", &update_time)))
		goto done;

	if (!(name = get_str_param("reservation_name", ctxt))) {
		resp_error(ctxt, ESLURM_RESERVATION_INVALID, __func__,
			   "Reservation name is requied for singular query");
		goto done;
	}

	errno = 0;
	if ((rc = slurm_load_reservations(update_time, &res_info_ptr))) {
		if (rc == SLURM_ERROR)
			rc = errno;

		resp_error(ctxt, rc, "slurm_load_reservations()",
			   "Unable to query reservation %s", name);

		goto done;
	}

	if (!res_info_ptr || (res_info_ptr->record_count == 0)) {
		resp_error(ctxt, ESLURM_RESERVATION_INVALID, __func__,
			   "Unable to query reservation %s", name);
		goto done;
	}

	for (int i = 0; !rc && i < res_info_ptr->record_count; i++) {
		if (!xstrcasecmp(name,
				 res_info_ptr->reservation_array[i].name)) {
			res = &res_info_ptr->reservation_array[i];
			break;
		}
	}

	if (!res) {
		resp_error(ctxt, ESLURM_REST_INVALID_QUERY, __func__,
			   "Unable to find reservation %s", name);
	} else {
		reserve_info_msg_t r = {
			.last_update = res_info_ptr->last_update,
			.record_count = 1,
			.reservation_array = res,
		};
		DUMP_OPENAPI_RESP_SINGLE(OPENAPI_RESERVATION_RESP, &r, ctxt);
	}

done:
	slurm_free_reservation_info_msg(res_info_ptr);
	return fini_connection(ctxt);
}

extern void init_op_reservations(void)
{
	bind_handler("/slurm/{data_parser}/reservations/",
		     _op_handler_reservations);
	bind_handler("/slurm/{data_parser}/reservation/{reservation_name}",
		     _op_handler_reservation);
}

extern void destroy_op_reservations(void)
{
	unbind_operation_handler(_op_handler_reservations);
}
