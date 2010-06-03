#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <util.h>
#include <config.h>
#include <ecl_util.h>
#include <enkf_macros.h>
#include <gen_data_config.h>
#include <enkf_types.h>
#include <pthread.h>
#include <path_fmt.h>
#include <gen_data_common.h>
#include <active_list.h>
#include <int_vector.h>

#define GEN_DATA_CONFIG_ID 90051
struct gen_data_config_struct {
  CONFIG_STD_FIELDS;
  char                         * key;                   /* The key this gen_data instance is known under - needed for debugging. */
  ecl_type_enum  	         internal_type;         /* The underlying type (float | double) of the data in the corresponding gen_data instances. */
  char                         * template_file;        
  char           	       * template_buffer;       /* Buffer containing the content of the template - read and internalized at boot time. */
  char                         * template_key;
  int            	         template_data_offset;  /* The offset into the template buffer before the data should come. */
  int            	         template_data_skip;    /* The length of data identifier in the template.*/ 
  int            	         template_buffer_size;  /* The total size (bytes) of the template buffer .*/
  path_fmt_type  	       * init_file_fmt;         /* file format for the file used to load the inital values - NULL if the instance is initialized from the forward model. */
  gen_data_file_format_type    	 input_format;          /* The format used for loading gen_data instances when the forward model has completed *AND* for loading the initial files.*/
  gen_data_file_format_type    	 output_format;         /* The format used when gen_data instances are written to disk for the forward model. */
  active_list_type             * active_list;           /* List of (EnKF) active indices. */
  pthread_mutex_t                update_lock;           /* mutex serializing (write) access to the gen_data_config object. */
  int_vector_type              * data_size_vector;      /* Data size, i.e. number of elements , indexed with report_step */
};

/*****************************************************************/

UTIL_SAFE_CAST_FUNCTION(gen_data_config , GEN_DATA_CONFIG_ID)
UTIL_SAFE_CAST_FUNCTION_CONST(gen_data_config , GEN_DATA_CONFIG_ID)

gen_data_file_format_type gen_data_config_get_input_format ( const gen_data_config_type * config) { return config->input_format; }
gen_data_file_format_type gen_data_config_get_output_format( const gen_data_config_type * config) { return config->output_format; }


ecl_type_enum gen_data_config_get_internal_type(const gen_data_config_type * config) {
  return config->internal_type;
}


/**
   If current_size as queried from config->data_size_vector == -1
   (i.e. not set); we seek through 
*/

int gen_data_config_get_data_size( const gen_data_config_type * config , int report_step) {
  int current_size = int_vector_safe_iget( config->data_size_vector , report_step );
  if (current_size < 0) 
    util_abort("%s: Size not set for object:%s report_step:%d - internal error: \n",__func__ , config->key , report_step);
  return current_size; 
}


int gen_data_config_get_byte_size( const gen_data_config_type * config , int report_step) {
  int byte_size = gen_data_config_get_data_size( config , report_step ) * ecl_util_get_sizeof_ctype( config->internal_type );
  return byte_size;
}



gen_data_config_type * gen_data_config_alloc_empty( const char * key ) {
  gen_data_config_type * config = util_malloc(sizeof * config , __func__);
  UTIL_TYPE_ID_INIT( config , GEN_DATA_CONFIG_ID);

  config->key               = util_alloc_string_copy( key );
  config->init_file_fmt     = NULL;
  config->template_file     = NULL;
  config->template_buffer   = NULL;
  config->template_key      = NULL;
  config->data_size  	    = 0;
  config->internal_type     = ECL_DOUBLE_TYPE;
  config->active_list       = active_list_alloc( ALL_ACTIVE );
  config->input_format      = GEN_DATA_UNDEFINED;
  config->output_format     = GEN_DATA_UNDEFINED;
  config->data_size_vector  = int_vector_alloc( 0 , -1 );   /* The default value: -1 - indicates "NOT SET" */
  pthread_mutex_init( &config->update_lock , NULL );

  return config;
}


static void gen_data_config_set_init_file_fmt( gen_data_config_type * gen_data_config , const char * init_file_fmt ) {
  gen_data_config->init_file_fmt = path_fmt_realloc_path_fmt( gen_data_config->init_file_fmt , init_file_fmt );
}


const char * gen_data_config_get_init_file_fmt( const gen_data_config_type * config ) {
  return path_fmt_get_fmt( config->init_file_fmt );
}



static void gen_data_config_set_template( gen_data_config_type * config , const char * template_ecl_file , const char * template_data_key ) {
  util_safe_free( config->template_buffer ); 
  config->template_buffer = NULL;

  if (template_ecl_file != NULL) {
    char *data_ptr;
    config->template_buffer = util_fread_alloc_file_content( template_ecl_file , &config->template_buffer_size);
    data_ptr = strstr(config->template_buffer , template_data_key);
    if (data_ptr == NULL) 
      util_abort("%s: template:%s can not be used - could not find data key:%s \n",__func__ , template_ecl_file , template_data_key);
    else {
      config->template_data_offset = data_ptr - config->template_buffer;
      config->template_data_skip   = strlen( template_data_key );
    }
  } else 
    config->template_buffer = NULL;
  config->template_file = util_realloc_string_copy( config->template_file , template_ecl_file );
  config->template_key  = util_realloc_string_copy( config->template_key , template_data_key );
}


const char * gen_data_config_get_template_file( const gen_data_config_type * config ) {
  return config->template_file;
}

const char * gen_data_config_get_template_key( const gen_data_config_type * config ) {
  return config->template_key;
}


static void gen_data_config_set_io_format( gen_data_config_type * config , gen_data_file_format_type output_format , gen_data_file_format_type input_format) {
  
  config->output_format = output_format;
  config->input_format  = input_format;
}



/**
   Observe that all the consistency checks are in thise functions, and not in
   the various small static functions called by this function, it is therefor
   important that only this full function is used, and not the small individual
   (static for a reason ...) functions.

   Observe that the checks on == NULL and != NULL for the various parameters
   should already have been performed (in enkf_config_node_update_gen_data).
*/

void gen_data_config_update(gen_data_config_type * config           , 
                            enkf_var_type var_type                  , /* This is ONLY included too be able to do a sensible consistency check. */
                            gen_data_file_format_type input_format  ,
                            gen_data_file_format_type output_format ,
                            const char * init_file_fmt              ,  
                            const char * template_ecl_file          , 
                            const char * template_data_key) {

  
  if ((var_type != DYNAMIC_RESULT) && (output_format == GEN_DATA_UNDEFINED))
    util_abort("%s: When specifying an enkf output file you must specify an output format as well. \n",__func__);
  
  if ((var_type != PARAMETER) && (input_format == GEN_DATA_UNDEFINED))
    util_abort("%s: When specifying a file for ERT to load from - you must also specify the format of the loaded files. \n",__func__);
  
  if (input_format == ASCII_TEMPLATE)
    util_abort("%s: Format ASCII_TEMPLATE is not valid as INPUT_FORMAT \n",__func__);

  if ((template_ecl_file != NULL) && (template_data_key == NULL))
    util_abort("%s: When using a template you MUST also set a template_key \n",__func__);

  /*****************************************************************/

  gen_data_config_set_template( config , template_ecl_file , template_data_key );
  gen_data_config_set_init_file_fmt( config , init_file_fmt );
  gen_data_config_set_io_format( config , output_format , input_format);

  /*****************************************************************/

  if ((output_format == ASCII_TEMPLATE) && (config->template_buffer == NULL))
    util_abort("%s: When specifying output_format == ASCII_TEMPLATE you must also supply a template_ecl_file\n",__func__);

}




/**
   This function takes a string representation of one of the
   gen_data_file_format_type values, and returns the corresponding
   integer value.
   
   Will return gen_data_undefined if the string is not recognized,
   calling scope must check on this return value.
*/


gen_data_file_format_type gen_data_config_check_format( const void * format_string ) {
  gen_data_file_format_type type = GEN_DATA_UNDEFINED;
  
  if (format_string != NULL) {
    
    if (strcmp(format_string , "ASCII") == 0)
      type = ASCII;
    else if (strcmp(format_string , "ASCII_TEMPLATE") == 0)
      type = ASCII_TEMPLATE;
    else if (strcmp(format_string , "BINARY_DOUBLE") == 0)
      type = BINARY_DOUBLE;
    else if (strcmp(format_string , "BINARY_FLOAT") == 0)
      type = BINARY_FLOAT;
    
    if (type == GEN_DATA_UNDEFINED)
      util_exit("Sorry: format:\"%s\" not recognized - valid values: ASCII / ASCII_TEMPLATE / BINARY_DOUBLE / BINARY_FLOAT \n", format_string);
  }
  
  return type;
}


/**
   The valid options are:

   INPUT_FORMAT:(ASCII|ASCII_TEMPLATE|BINARY_DOUBLE|BINARY_FLOAT)
   OUTPUT_FORMAT:(ASCII|ASCII_TEMPLATE|BINARY_DOUBLE|BINARY_FLOAT)
   INIT_FILES:/some/path/with/%d
   TEMPLATE:/some/template/file
   KEY:<SomeKeyFoundInTemplate>
   ECL_FILE:<filename to write EnKF ==> Forward model>  (In the case of gen_param - this is extracted in the calling scope).
   RESULT_FILE:<filename to read EnKF <== Forward model> 

*/

//gen_data_config_type * gen_data_config_alloc_with_options(const char * key , bool as_param , const stringlist_type * options) {
//  const ecl_type_enum internal_type = ECL_DOUBLE_TYPE;
//  gen_data_config_type * config;
//  hash_type * opt_hash              = hash_alloc_from_options( options );
//  
//  config = gen_data_config_alloc(key      , 
//                                 as_param , 
//                                 internal_type , 
//                                 __gen_data_config_check_format( hash_safe_get( opt_hash , "INPUT_FORMAT") ),
//                                 __gen_data_config_check_format( hash_safe_get( opt_hash , "OUTPUT_FORMAT") ),
//                                 hash_safe_get( opt_hash , "INIT_FILES" ),
//                                 hash_safe_get( opt_hash , "TEMPLATE" ),
//                                 hash_safe_get( opt_hash , "KEY"),
//                                 hash_safe_get( opt_hash , "ECL_FILE"),
//                                 hash_safe_get( opt_hash , "RESULT_FILE"),
//                                 hash_safe_get( opt_hash , "MIN_STD" ));
//  
//  hash_free(opt_hash);
//  return config;
//}




void gen_data_config_free(gen_data_config_type * config) {
  active_list_free(config->active_list);
  if (config->init_file_fmt != NULL) path_fmt_free(config->init_file_fmt);
  int_vector_free( config->data_size_vector );
  
  util_safe_free( config->key );
  util_safe_free( config->template_buffer );
  util_safe_free( config->template_file );
  util_safe_free( config->template_key );

  free(config);
}




/**
   This function gets a size (from a gen_data) instance, and verifies
   that the size agrees with the currently stored size and
   report_step. If the report_step is new we just recordthe new info,
   otherwise it will break hard.
*/


/**
   Does not work properly with:
   
   1. keep_run_path - the load_file will be left hanging around - and loaded again and again.
   2. Doing forward several steps - how to (time)index the files?
     
*/


void gen_data_config_assert_size(gen_data_config_type * config , int data_size, int report_step) {
  pthread_mutex_lock( &config->update_lock );
  {
    int current_size = int_vector_safe_iget( config->data_size_vector , report_step );
    if (current_size < 0) {
      int_vector_iset( config->data_size_vector , report_step , data_size );
      current_size = data_size;
    }

    if (current_size != data_size) {
      util_abort("%s: Size mismatch when loading:%s from file - got %d elements - expected:%d [report_step:%d] \n",
		 __func__ , 
                 gen_data_config_get_key( config ),
		 data_size , 
		 current_size , 
		 report_step);
    }
  }
  pthread_mutex_unlock( &config->update_lock );
}





char * gen_data_config_alloc_initfile(const gen_data_config_type * config , int iens) {
  if (config->init_file_fmt != NULL) {
    char * initfile = path_fmt_alloc_path(config->init_file_fmt , false , iens);
    return initfile;
  } else 
    return NULL; 
}




void gen_data_config_get_template_data( const gen_data_config_type * config , 
					char ** template_buffer    , 
					int * template_data_offset , 
					int * template_buffer_size , 
					int * template_data_skip) {
  
  *template_buffer      = config->template_buffer;
  *template_data_offset = config->template_data_offset;
  *template_buffer_size = config->template_buffer_size;
  *template_data_skip   = config->template_data_skip;
  
}






const char * gen_data_config_get_key( const gen_data_config_type * config) {
  return config->key;
}


/*****************************************************************/

VOID_FREE(gen_data_config)
GET_ACTIVE_LIST(gen_data)
