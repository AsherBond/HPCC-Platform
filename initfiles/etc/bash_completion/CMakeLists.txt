################################################################################
#    HPCC SYSTEMS software Copyright (C) 2012 HPCC Systems®.
#
#    All rights reserved. This program is free software: you can redistribute it
#    and/or modify
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
################################################################################
FOREACH( oFILES
    ${CMAKE_CURRENT_SOURCE_DIR}/ecl
    ${CMAKE_CURRENT_SOURCE_DIR}/eclcc
    ${CMAKE_CURRENT_SOURCE_DIR}/eclplus
    ${CMAKE_CURRENT_SOURCE_DIR}/eclagent
    ${CMAKE_CURRENT_SOURCE_DIR}/esp-api
)
    install ( FILES ${oFILES} DESTINATION etc/bash_completion.d COMPONENT Runtime )
ENDFOREACH ( oFILES )
