/*******************************************************************************
 * Copyright (C) 2004-2006 Intel Corp. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 *  - Neither the name of Intel Corp. nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL Intel Corp. OR THE CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *******************************************************************************/

/**
 * @author Anas Nashif
 * @author Eugene Yarmosh
 * @author Vadim Revyakin
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <errno.h>
#include <time.h>

#include <u/libu.h>
#include <wsman-client-api.h>
#include <wsman-debug.h>
#include "wsman-client-options.h"


static void 
wsman_output(WsManClient *cl, WsXmlDocH doc)
{
	FILE           *f = stdout;
	const char     *filename = wsman_options_get_output_file();
	WS_LASTERR_Code err;

	err = wsman_client_get_last_error(cl);
	if (err != WS_LASTERR_OK) {
		return;
	}
	if (!doc) {
		error("doc with NULL content");
		return;
	}
	if (filename) {
		f = fopen(filename, "w+");
		if (f == NULL) {
			error("Could not open file for writing");
			return;
		}
	}
	ws_xml_dump_node_tree(f, ws_xml_get_doc_root(doc));
	if (f != stdout) {
		fclose(f);
	}
	return;
}


static void
initialize_logging(void)
{
	debug_add_handler(wsman_debug_message_handler, DEBUG_LEVEL_ALWAYS, NULL);
}



int 
main(int argc, char **argv)
{
	int             retVal = 0;
	int             op;
	char           *filename;
	dictionary     *ini = NULL;
	WsManClient    *cl;
	WsXmlDocH       doc;
	char           *enumContext;
	WsXmlDocH       rqstDoc;
	actionOptions   options;
	WsXmlDocH       enum_response;
	WsXmlDocH		resource;
	char           *enumeration_mode, *binding_enumeration_mode, *resource_uri_with_selectors;
	char           *resource_uri = NULL;



	filename = (char *) wsman_options_get_config_file();
	if (filename) {
		ini = iniparser_load(filename);
		if (ini == NULL) {
			fprintf(stderr, "cannot parse file [%s]", filename);
			exit(EXIT_FAILURE);
		} else if (!wsman_read_client_config(ini)) {
			fprintf(stderr, "Configuration file not found\n");
			exit(EXIT_FAILURE);
		}
	}
	if (!wsman_parse_options(argc, argv)) {
		exit(EXIT_FAILURE);
	}
	wsman_setup_transport_and_library_options();

	initialize_logging();
	//	wsman_client_transport_init(NULL);
	initialize_action_options(&options);

	debug("Certificate: %s", wsman_options_get_cafile());

	cl = wsman_create_client(wsman_options_get_server(),
			wsman_options_get_server_port(),
			wsman_options_get_path(),
			wsman_options_get_cafile() ? "https" : "http",
			wsman_options_get_username(),
			wsman_options_get_password());



	if (cl == NULL) {
		error("Null Client");
		exit(EXIT_FAILURE);
	}
	/*
	 * Setup Resource URI and Selectors
	 */
	resource_uri_with_selectors = wsman_options_get_resource_uri();
	if (resource_uri_with_selectors) {
		wsman_set_options_from_uri(resource_uri_with_selectors, &options);
		wsman_remove_query_string(resource_uri_with_selectors, &resource_uri);
	}
	op = wsman_options_get_action();

	if (wsman_options_get_dump_request()) {
		wsman_set_action_option(&options, FLAG_DUMP_REQUEST);
	}
	if (wsman_options_get_max_envelope_size()) {
		options.max_envelope_size = wsman_options_get_max_envelope_size();
	}
	if (wsman_options_get_operation_timeout()) {
		options.timeout = wsman_options_get_operation_timeout();
	}
	if (wsman_options_get_fragment()) {
		options.fragment = wsman_options_get_fragment();
	}
	if (wsman_options_get_filter()) {
		options.filter = wsman_options_get_filter();
	}
	if (wsman_options_get_dialect()) {
		options.dialect = wsman_options_get_dialect();
	}
	options.properties = wsman_options_get_properties();
	options.cim_ns = wsman_options_get_cim_namespace();


	switch (op) {
	case WSMAN_ACTION_TEST:
		rqstDoc = wsman_client_read_file(cl,
				wsman_options_get_input_file(), "UTF-8", 0);
		wsman_send_request(cl, rqstDoc);
		doc = wsman_build_envelope_from_response(cl);
		wsman_output(cl, doc);
		if (doc) {
			ws_xml_destroy_doc(doc);
		}
		break;
	case WSMAN_ACTION_IDENTIFY:
		doc = wsman_identify(cl, options);
		wsman_output(cl, doc);
		if (doc) {
			ws_xml_destroy_doc(doc);
		}
		break;
	case WSMAN_ACTION_CUSTOM:

		doc = wsman_invoke(cl, resource_uri, options,
				wsman_options_get_invoke_method(), NULL);
		wsman_output(cl, doc);
		if (doc) {
			ws_xml_destroy_doc(doc);
		}
		break;
	case WSMAN_ACTION_TRANSFER_DELETE:
		doc = ws_transfer_delete(cl, resource_uri, options);
		wsman_output(cl, doc);
		if (doc) {
			ws_xml_destroy_doc(doc);
		}
		break;		
	case WSMAN_ACTION_TRANSFER_CREATE:
		if (wsman_options_get_input_file()) {
			resource = wsman_client_read_file(cl,
					wsman_options_get_input_file(), "UTF-8", 0);
			doc = ws_transfer_create(cl, resource_uri, options, resource);
			ws_xml_destroy_doc(resource);
			wsman_output(cl, doc);
			if (doc) {
				ws_xml_destroy_doc(doc);
			} 
		} else {
			fprintf(stderr, "Missing resource data\n");
		}
		break;
	case WSMAN_ACTION_TRANSFER_PUT:
		if (wsman_options_get_input_file()) {
			printf("input file provided\n");
			resource = wsman_client_read_file(cl,
					wsman_options_get_input_file(), "UTF-8", 0);
			doc = ws_transfer_put(cl, resource_uri, options, resource);
			ws_xml_destroy_doc(resource);
		} else {
			doc = ws_transfer_get_and_put(cl, resource_uri, options);
		}
		wsman_output(cl, doc);
		if (doc) {
			ws_xml_destroy_doc(doc);
		}
		break;
	case WSMAN_ACTION_TRANSFER_GET:
		doc = ws_transfer_get(cl, resource_uri, options);
		wsman_output(cl, doc);
		if (doc) {
			ws_xml_destroy_doc(doc);
		}
		break;
	case WSMAN_ACTION_PULL:
		doc = wsenum_pull(cl, resource_uri, options, wsman_options_get_enum_context());
		wsman_output(cl, doc);
		if (doc) {
			ws_xml_destroy_doc(doc);
		}
		break;
	case WSMAN_ACTION_RELEASE:
		doc = wsenum_release(cl, resource_uri, options, wsman_options_get_enum_context());
		wsman_output(cl, doc);
		if (doc) {
			ws_xml_destroy_doc(doc);
		}
		break;
	case WSMAN_ACTION_ENUMERATION:

		enumeration_mode = wsman_options_get_enum_mode();
		binding_enumeration_mode = wsman_options_get_binding_enum_mode();

		if (enumeration_mode) {
			if (strcmp(enumeration_mode, "epr") == 0)
				wsman_set_action_option(&options, FLAG_ENUMERATION_ENUM_EPR);
			else
				wsman_set_action_option(&options, FLAG_ENUMERATION_ENUM_OBJ_AND_EPR);
		}
		if (binding_enumeration_mode) {
			if (strcmp(binding_enumeration_mode, "include") == 0)
				wsman_set_action_option(&options, FLAG_IncludeSubClassProperties);
			else if (strcmp(binding_enumeration_mode, "exclude") == 0)
				wsman_set_action_option(&options, FLAG_ExcludeSubClassProperties);
			else if (strcmp(binding_enumeration_mode, "none") == 0)
				wsman_set_action_option(&options, FLAG_POLYMORPHISM_NONE);
		}
		if (wsman_options_get_optimize_enum()) {
			wsman_set_action_option(&options, FLAG_ENUMERATION_OPTIMIZATION);
		}
		options.max_elements = wsman_options_get_max_elements();

		if (wsman_options_get_estimate_enum()) {
			wsman_set_action_option(&options, FLAG_ENUMERATION_COUNT_ESTIMATION);
		}
		enum_response = wsenum_enumerate(cl,
				resource_uri, options);
		wsman_output(cl, enum_response);
		if (enum_response) {
			if (!(wsman_client_get_response_code(cl) == 200 ||
						wsman_client_get_response_code(cl) == 400 ||
						wsman_client_get_response_code(cl) == 500)) {
				break;
			}
			enumContext = wsenum_get_enum_context(enum_response);
			ws_xml_destroy_doc(enum_response);
		} else {
			break;
		}

		if (!wsman_options_get_step_request()) {
			while (enumContext != NULL && enumContext[0] != 0) {

				doc = wsenum_pull(cl, resource_uri, options, enumContext);
				wsman_output(cl, doc);

				if (wsman_client_get_response_code(cl) != 200 &&
						wsman_client_get_response_code(cl) != 400 &&
						wsman_client_get_response_code(cl) != 500) {
					break;
				}
				enumContext = wsenum_get_enum_context(doc);
				if (doc) {
					ws_xml_destroy_doc(doc);
				}
			}
		}
		break;
	default:
		fprintf(stderr, "Action not supported\n");
		retVal = 1;
	}


	if (wsman_client_get_response_code(cl) != 200) {
		fprintf(stderr, "Connection failed. response code = %ld\n",
				wsman_client_get_response_code(cl));
		if (wsman_client_get_fault_string(cl)) {
			fprintf(stderr, "%s\n", wsman_client_get_fault_string(cl));
		}
	}
	destroy_action_options(&options);
	wsman_release_client(cl);

	wsman_client_transport_fini();
	if (ini) {
		iniparser_freedict(ini);
	}
#ifdef DEBUG_VERBOSE
	printf("     ******   Transfer Time = %ull usecs ******\n", get_transfer_time());
#endif
	return retVal;

}
