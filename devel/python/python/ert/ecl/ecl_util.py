#  Copyright (C) 2011  Statoil ASA, Norway. 
#   
#  The file 'ecl_util.py' is part of ERT - Ensemble based Reservoir Tool. 
#   
#  ERT is free software: you can redistribute it and/or modify 
#  it under the terms of the GNU General Public License as published by 
#  the Free Software Foundation, either version 3 of the License, or 
#  (at your option) any later version. 
#   
#  ERT is distributed in the hope that it will be useful, but WITHOUT ANY 
#  WARRANTY; without even the implied warranty of MERCHANTABILITY or 
#  FITNESS FOR A PARTICULAR PURPOSE.   
#   
#  See the GNU General Public License at <http://www.gnu.org/licenses/gpl.html> 
#  for more details. 
"""
Constants from the header ecl_util.h - some stateless functions.

This module does not contain any class definitions; it mostly consists
of enum definitions/values from ecl_util.h; the enum values are
extracted from the shared library using the
ert.cwrap.cenum.create_enum() function in a semi-automagic manner.

In addition to the enum definitions there are a few stateless
functions from ecl_util.c which are not bound to any class type.
"""
import ctypes
from ert.cwrap import create_enum, CWrapper, CWrapperNameSpace, BaseCEnum
from ert.ecl import ECL_LIB

class EclFileEnum(BaseCEnum):
    ECL_OTHER_FILE = None
    ECL_RESTART_FILE = None
    ECL_UNIFIED_RESTART_FILE = None
    ECL_SUMMARY_FILE = None
    ECL_UNIFIED_SUMMARY_FILE = None
    ECL_GRID_FILE = None
    ECL_EGRID_FILE = None
    ECL_INIT_FILE = None
    ECL_RFT_FILE = None
    ECL_DATA_FILE = None


EclFileEnum.addEnum("ECL_OTHER_FILE", 0)
EclFileEnum.addEnum("ECL_RESTART_FILE", 1)
EclFileEnum.addEnum("ECL_UNIFIED_RESTART_FILE", 2)
EclFileEnum.addEnum("ECL_SUMMARY_FILE", 4)
EclFileEnum.addEnum("ECL_UNIFIED_SUMMARY_FILE", 8)
EclFileEnum.addEnum("ECL_SUMMARY_HEADER_FILE", 16)
EclFileEnum.addEnum("ECL_GRID_FILE", 32)
EclFileEnum.addEnum("ECL_EGRID_FILE", 64)
EclFileEnum.addEnum("ECL_INIT_FILE", 128)
EclFileEnum.addEnum("ECL_RFT_FILE", 256)
EclFileEnum.addEnum("ECL_DATA_FILE", 512)

EclFileEnum.registerEnum(ECL_LIB, "ecl_file_enum")

#-----------------------------------------------------------------

class EclPhaseEnum(BaseCEnum):
    ECL_OIL_PHASE = None
    ECL_GAS_PHASE = None
    ECL_WATER_PHASE = None

EclPhaseEnum.addEnum("ECL_OIL_PHASE" , 1 )
EclPhaseEnum.addEnum("ECL_GAS_PHASE" , 2 )
EclPhaseEnum.addEnum("ECL_WATER_PHASE" , 4 )

EclPhaseEnum.registerEnum(ECL_LIB, "ecl_phase_enum")

#-----------------------------------------------------------------

class EclTypeEnum(BaseCEnum):
    ECL_CHAR_TYPE   = None
    ECL_FLOAT_TYPE  = None
    ECL_DOUBLE_TYPE = None
    ECL_INT_TYPE    = None
    ECL_BOOL_TYPE   = None
    ECL_MESS_TYPE   = None
  
EclTypeEnum.addEnum("ECL_CHAR_TYPE" , 0 )
EclTypeEnum.addEnum("ECL_FLOAT_TYPE" , 1 )
EclTypeEnum.addEnum("ECL_DOUBLE_TYPE" , 2 )
EclTypeEnum.addEnum("ECL_INT_TYPE" , 3 )
EclTypeEnum.addEnum("ECL_BOOL_TYPE" , 4 )
EclTypeEnum.addEnum("ECL_MESS_TYPE" , 5 )


EclTypeEnum.registerEnum(ECL_LIB, "ecl_type_enum")

#-----------------------------------------------------------------

class EclFileFlagEnum(BaseCEnum):
    ECL_FILE_CLOSE_STREAM = None
    ECL_FILE_WRITABLE = None

EclFileFlagEnum.addEnum("ECL_FILE_CLOSE_STREAM" , 1 )
EclFileFlagEnum.addEnum("ECL_FILE_WRITABLE" , 2 )


EclFileFlagEnum.registerEnum(ECL_LIB, "ecl_file_flag_enum")


#-----------------------------------------------------------------

cwrapper = CWrapper(ECL_LIB)
cfunc = CWrapperNameSpace("ecl_util")

cfunc.get_num_cpu = cwrapper.prototype("int ecl_util_get_num_cpu( char* )")
cfunc.get_file_type = cwrapper.prototype("ecl_file_enum ecl_util_get_file_type( char* , bool* , int*)")
cfunc.get_type_name = cwrapper.prototype("char* ecl_util_get_type_name( int )")
cfunc.get_start_date = cwrapper.prototype("time_t ecl_util_get_start_date( char* )")



class EclUtil(object):
    @staticmethod
    def get_num_cpu( datafile ):
        """
        Parse ECLIPSE datafile and determine how many CPUs are needed.

        Will look for the "PARALLELL" keyword, and then read off the
        number of CPUs required. Will return one if no PARALLELL keyword
        is found.
        """
        return cfunc.get_num_cpu(datafile)

    @staticmethod
    def get_file_type( filename ):
        """
        Will inspect an ECLIPSE filename and return an integer type flag.
        """
        file_type , fmt , step = EclUtil.inspectExtension( filename )
        return file_type
    
    @staticmethod
    def type_name(ecl_type):
        return cfunc.get_type_name(ecl_type)

    @staticmethod
    def get_start_date(datafile):
        return cfunc.get_start_date(datafile).datetime()

    @staticmethod
    def inspectExtension( filename ):
        """Will inspect an ECLIPSE filename and return a tuple consisting of
        file type (EclFileEnum), a bool for formatted or not, and an
        integer for the step number.

        """
        fmt_file = ctypes.c_bool()
        report_step = ctypes.c_int(-1)
        file_type = cfunc.get_file_type(filename, ctypes.byref(fmt_file) , ctypes.byref(report_step))
        if report_step.value == -1:
            step = None
        else:
            step = report_step.value

        return (file_type , fmt_file.value , step)
        
        

get_num_cpu = EclUtil.get_num_cpu
get_file_type = EclUtil.get_file_type
type_name = EclUtil.type_name
get_start_date = EclUtil.get_start_date
