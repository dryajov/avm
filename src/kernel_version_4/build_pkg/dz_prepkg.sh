#!/bin/bash
# All the paths in this file are relative to DZ_PROJECT_SOURCE_PATH
# All package paths below are relative to DZ_PACKAGE_DIR
DZ_BIN_DIR_NAME="bin"
DZ_LOGS_DIR_NAME="logs"
DZ_DOCS_DIR_NAME="docs"
DZ_KERNEL_DIR_NAME="kernel"
DZ_ETC_DIR_NAME="etc"
DZ_LICENSE_DIR_NAME="license"
DZ_HEADERS_DIR_NAME="headers"
DZ_SCRIPTS_DIR_NAME="scripts"
DZ_PRIVATE_DIR_NAME=".tmp"
DZ_MODULE_AVM="avm"
DZ_MODULE_LIO="lio"
DZ_MODULE_DM="dm" #For device mapper
DZ_MODULE_AVM_DIR=${DZ_PACKAGE_DIR}/${DZ_MODULE_AVM}
DZ_MODULE_LIO_DIR=${DZ_PACKAGE_DIR}/${DZ_MODULE_LIO}
DZ_MODULE_DM_DIR=${DZ_PACKAGE_DIR}/${DZ_MODULE_DM_DIR}

#===============================================================================
# Main
#===============================================================================
# Pre-requisites
echo "Creating AVM directores inside RPM package"
mkdir -p ${DZ_PACKAGE_DIR}/${DZ_MODULE_AVM}/${DZ_BIN_DIR_NAME} 					|| exit 1
mkdir -p ${DZ_PACKAGE_DIR}/${DZ_MODULE_AVM}/${DZ_LOGS_DIR_NAME} 				|| exit 1
mkdir -p ${DZ_PACKAGE_DIR}/${DZ_MODULE_AVM}/${DZ_DOCS_DIR_NAME} 				|| exit 1
mkdir -p ${DZ_PACKAGE_DIR}/${DZ_MODULE_AVM}/${DZ_KERNEL_DIR_NAME} 				|| exit 1
mkdir -p ${DZ_PACKAGE_DIR}/${DZ_MODULE_AVM}/${DZ_ETC_DIR_NAME} 					|| exit 1
mkdir -p ${DZ_PACKAGE_DIR}/${DZ_MODULE_AVM}/${DZ_LICENSE_DIR_NAME} 				|| exit 1
mkdir -p ${DZ_PACKAGE_DIR}/${DZ_MODULE_AVM}/${DZ_HEADERS_DIR_NAME} 				|| exit 1
mkdir -p ${DZ_PACKAGE_DIR}/${DZ_MODULE_AVM}/${DZ_SCRIPTS_DIR_NAME} 				|| exit 1
mkdir -p ${DZ_PACKAGE_DIR}/${DZ_MODULE_AVM}/${DZ_PRIVATE_DIR_NAME} 				|| exit 1

echo "Creating LIO SCSI Target Stack directores inside RPM package"
mkdir -p ${DZ_PACKAGE_DIR}/${DZ_MODULE_LIO}/${DZ_BIN_DIR_NAME} 					|| exit 1
mkdir -p ${DZ_PACKAGE_DIR}/${DZ_MODULE_LIO}/${DZ_LOGS_DIR_NAME} 				|| exit 1
mkdir -p ${DZ_PACKAGE_DIR}/${DZ_MODULE_LIO}/${DZ_DOCS_DIR_NAME} 				|| exit 1
mkdir -p ${DZ_PACKAGE_DIR}/${DZ_MODULE_LIO}/${DZ_KERNEL_DIR_NAME} 				|| exit 1
mkdir -p ${DZ_PACKAGE_DIR}/${DZ_MODULE_LIO}/${DZ_ETC_DIR_NAME} 					|| exit 1
mkdir -p ${DZ_PACKAGE_DIR}/${DZ_MODULE_LIO}/${DZ_LICENSE_DIR_NAME} 				|| exit 1
mkdir -p ${DZ_PACKAGE_DIR}/${DZ_MODULE_LIO}/${DZ_HEADERS_DIR_NAME} 				|| exit 1
mkdir -p ${DZ_PACKAGE_DIR}/${DZ_MODULE_LIO}/${DZ_SCRIPTS_DIR_NAME} 				|| exit 1

echo "Creating Device Mapper directores inside RPM package"
mkdir -p ${DZ_PACKAGE_DIR}/${DZ_MODULE_DM}/${DZ_BIN_DIR_NAME} 		|| exit 1
mkdir -p ${DZ_PACKAGE_DIR}/${DZ_MODULE_DM}/${DZ_LOGS_DIR_NAME} 		|| exit 1
mkdir -p ${DZ_PACKAGE_DIR}/${DZ_MODULE_DM}/${DZ_DOCS_DIR_NAME} 		|| exit 1
mkdir -p ${DZ_PACKAGE_DIR}/${DZ_MODULE_DM}/${DZ_KERNEL_DIR_NAME} 	|| exit 1
mkdir -p ${DZ_PACKAGE_DIR}/${DZ_MODULE_DM}/${DZ_ETC_DIR_NAME} 		|| exit 1
mkdir -p ${DZ_PACKAGE_DIR}/${DZ_MODULE_DM}/${DZ_LICENSE_DIR_NAME} 	|| exit 1
mkdir -p ${DZ_PACKAGE_DIR}/${DZ_MODULE_DM}/${DZ_HEADERS_DIR_NAME} 	|| exit 1
mkdir -p ${DZ_PACKAGE_DIR}/${DZ_MODULE_DM}/${DZ_SCRIPTS_DIR_NAME} 	|| exit 1

# Copy all the files to be shipped

#Copy Drivers/Kernel Modules files
cp -r ${DZ_PROJECT_SOURCE_PATH}/kernel_objects/*.ko 					${DZ_MODULE_AVM_DIR}/${DZ_KERNEL_DIR_NAME}/
cp -r ${DZ_PROJECT_SOURCE_PATH}/infra_engine/*.o 						${DZ_MODULE_AVM_DIR}/${DZ_KERNEL_DIR_NAME}/
cp -r ${DZ_PROJECT_SOURCE_PATH}/alignment_engine/*.o 					${DZ_MODULE_AVM_DIR}/${DZ_KERNEL_DIR_NAME}/
cp -r ${DZ_PROJECT_SOURCE_PATH}/target_engine/*.o						${DZ_MODULE_AVM_DIR}/${DZ_KERNEL_DIR_NAME}/
cp -r ${DZ_PROJECT_SOURCE_PATH}/bec_engine/*.o 							${DZ_MODULE_AVM_DIR}/${DZ_KERNEL_DIR_NAME}/
cp -r ${DZ_PROJECT_SOURCE_PATH}/fec_engine/*.o 							${DZ_MODULE_AVM_DIR}/${DZ_KERNEL_DIR_NAME}/
cp -r ${DZ_PROJECT_SOURCE_PATH}/volume_engine/*.o 						${DZ_MODULE_AVM_DIR}/${DZ_KERNEL_DIR_NAME}/
cp -r ${DZ_PROJECT_SOURCE_PATH}/dedupe_engine/*.o 						${DZ_MODULE_AVM_DIR}/${DZ_KERNEL_DIR_NAME}/
cp -r ${DZ_PROJECT_SOURCE_PATH}/metadata_engine/*.o 					${DZ_MODULE_AVM_DIR}/${DZ_KERNEL_DIR_NAME}/

#Copy License file
cp -r ${DZ_PROJECT_SOURCE_PATH}/License.txt 							${DZ_MODULE_AVM_DIR}/${DZ_LICENSE_DIR_NAME}/

#Copy Header files
cp -r ${DZ_PROJECT_SOURCE_PATH}/headers/*.h 							${DZ_MODULE_AVM_DIR}/${DZ_HEADERS_DIR_NAME}/
cp -r ${DZ_PROJECT_SOURCE_PATH}/infra_engine/infra_engine_bec.c 		${DZ_MODULE_AVM_DIR}/${DZ_HEADERS_DIR_NAME}/
cp -r ${DZ_PROJECT_SOURCE_PATH}/infra_engine/infra_engine_fec.c 		${DZ_MODULE_AVM_DIR}/${DZ_HEADERS_DIR_NAME}/
cp -r ${DZ_PROJECT_SOURCE_PATH}/infra_engine/infra_engine_dedupe.c 		${DZ_MODULE_AVM_DIR}/${DZ_HEADERS_DIR_NAME}/
cp -r ${DZ_PROJECT_SOURCE_PATH}/infra_engine/infra_engine_metadata.c 	${DZ_MODULE_AVM_DIR}/${DZ_HEADERS_DIR_NAME}/
cp -r ${DZ_PROJECT_SOURCE_PATH}/infra_engine/infra_engine_target.c 		${DZ_MODULE_AVM_DIR}/${DZ_HEADERS_DIR_NAME}/

#Copy config file
cp -r ${DZ_PROJECT_SOURCE_PATH}/scripts/*.conf 							${DZ_MODULE_AVM_DIR}/${DZ_ETC_DIR_NAME}/

#Copy documentation file
cp -r ${DZ_PROJECT_SOURCE_PATH}/cli_engine/*.cli 						${DZ_MODULE_AVM_DIR}/${DZ_DOCS_DIR_NAME}/

#Copy CLI Files
cp -r ${DZ_PROJECT_SOURCE_PATH}/cli_engine/AISA_Error.py 				${DZ_MODULE_AVM_DIR}/${DZ_BIN_DIR_NAME}/
cp -r ${DZ_PROJECT_SOURCE_PATH}/cli_engine/AISA_Defaults.py 			${DZ_MODULE_AVM_DIR}/${DZ_BIN_DIR_NAME}/
cp -r ${DZ_PROJECT_SOURCE_PATH}/cli_engine/AISA_Utils.py 				${DZ_MODULE_AVM_DIR}/${DZ_BIN_DIR_NAME}/
cp -r ${DZ_PROJECT_SOURCE_PATH}/cli_engine/AISA_Lio.py		 				${DZ_MODULE_AVM_DIR}/${DZ_BIN_DIR_NAME}/
cp -r ${DZ_PROJECT_SOURCE_PATH}/cli_engine/AISA_MDRaid.py 				${DZ_MODULE_AVM_DIR}/${DZ_BIN_DIR_NAME}/
cp -r ${DZ_PROJECT_SOURCE_PATH}/cli_engine/AISA_AVMCore.py 				${DZ_MODULE_AVM_DIR}/${DZ_BIN_DIR_NAME}/
cp -r ${DZ_PROJECT_SOURCE_PATH}/cli_engine/AISA_AVMGlobals.py 			${DZ_MODULE_AVM_DIR}/${DZ_BIN_DIR_NAME}/
cp -r ${DZ_PROJECT_SOURCE_PATH}/cli_engine/AISA_AVMHelp.py 				${DZ_MODULE_AVM_DIR}/${DZ_BIN_DIR_NAME}/
cp -r ${DZ_PROJECT_SOURCE_PATH}/cli_engine/AISA_AVMConfig.py 				${DZ_MODULE_AVM_DIR}/${DZ_BIN_DIR_NAME}/
cp -r ${DZ_PROJECT_SOURCE_PATH}/cli_engine/AISA_Avm.py 					${DZ_MODULE_AVM_DIR}/${DZ_BIN_DIR_NAME}/

#Copy nbcat utility and rename as dzcat for reading log file
cp -r ${DZ_PROJECT_SOURCE_PATH}/log_engine/nbcat 						${DZ_MODULE_AVM_DIR}/${DZ_BIN_DIR_NAME}/dzcat

#Copy useful scripts
cp -r ${DZ_PROJECT_SOURCE_PATH}/scripts/avmconf.sh 						${DZ_MODULE_AVM_DIR}/${DZ_SCRIPTS_DIR_NAME}/
cp -r ${DZ_PROJECT_SOURCE_PATH}/scripts/avmlog.sh 						${DZ_MODULE_AVM_DIR}/${DZ_SCRIPTS_DIR_NAME}/
cp -r ${DZ_PROJECT_SOURCE_PATH}/scripts/force_reboot.sh 				${DZ_MODULE_AVM_DIR}/${DZ_SCRIPTS_DIR_NAME}/
cp -r ${DZ_PROJECT_SOURCE_PATH}/scripts/force_shutdown.sh 				${DZ_MODULE_AVM_DIR}/${DZ_SCRIPTS_DIR_NAME}/
cp -r ${DZ_PROJECT_SOURCE_PATH}/scripts/iscsi_discovery.sh 				${DZ_MODULE_AVM_DIR}/${DZ_SCRIPTS_DIR_NAME}/
cp -r ${DZ_PROJECT_SOURCE_PATH}/scripts/crash_kernel.sh 				${DZ_MODULE_AVM_DIR}/${DZ_SCRIPTS_DIR_NAME}/
cp -r ${DZ_PROJECT_SOURCE_PATH}/scripts/firewall.sh 					${DZ_MODULE_AVM_DIR}/${DZ_SCRIPTS_DIR_NAME}/
cp -r ${DZ_PROJECT_SOURCE_PATH}/scripts/add_firewall.py 				${DZ_MODULE_AVM_DIR}/${DZ_SCRIPTS_DIR_NAME}/
cp -r ${DZ_PROJECT_SOURCE_PATH}/scripts/makefile.testvm 				${DZ_MODULE_AVM_DIR}/${DZ_SCRIPTS_DIR_NAME}/
cp -r ${DZ_PROJECT_SOURCE_PATH}/scripts/avm_start.sh 					${DZ_MODULE_AVM_DIR}/${DZ_SCRIPTS_DIR_NAME}/
cp -r ${DZ_PROJECT_SOURCE_PATH}/scripts/avm_stop.sh 					${DZ_MODULE_AVM_DIR}/${DZ_SCRIPTS_DIR_NAME}/
cp -r ${DZ_PROJECT_SOURCE_PATH}/scripts/avm_install.sh 					${DZ_MODULE_AVM_DIR}/${DZ_SCRIPTS_DIR_NAME}/
cp -r ${DZ_PROJECT_SOURCE_PATH}/scripts/avm_uninstall.sh 				${DZ_MODULE_AVM_DIR}/${DZ_SCRIPTS_DIR_NAME}/
cp -r ${DZ_PROJECT_SOURCE_PATH}/scripts/avm_load_modules.sh 			${DZ_MODULE_AVM_DIR}/${DZ_SCRIPTS_DIR_NAME}/
cp -r ${DZ_PROJECT_SOURCE_PATH}/scripts/avm_unload_modules.sh 			${DZ_MODULE_AVM_DIR}/${DZ_SCRIPTS_DIR_NAME}/


#####################################################################################
# NOTE:
# Backup source code for the time being. This should be removed once product is built
# NOTE:
#####################################################################################

echo  "Backup to hidden directory"
pdb_name="pdb_""`date +'%d_%m_%Y_%H_%M_%S'`.tar"
tar -cf ${DZ_PROJECT_SOURCE_PATH}/$pdb_name ${DZ_PROJECT_SOURCE_PATH}/* --exclude={*.ko,*.o,*.o.cmd,*.ko.cmd,*.mod.c,*.mod,*.rpm,*.tar}
cp  -r ${DZ_PROJECT_SOURCE_PATH}/$pdb_name ${DZ_MODULE_AVM_DIR}/${DZ_PRIVATE_DIR_NAME}/
rm -rf ${DZ_PROJECT_SOURCE_PATH}/$pdb_name

