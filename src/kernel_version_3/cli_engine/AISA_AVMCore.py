#! /usr/bin/python
from __future__ import print_function
import os
import sys
import getopt
import cmd
from lsm.lsmcli.cmdline import cmds
from sys import argv

from AISA_Utils 	import *
from AISA_Defaults 	import *
from AISA_AVMGlobals 	import *
#from AISA_Mdraid 	import *

#CLI_SUCCESS									= 0
#CLI_FAILURE									= -1
#CLI_NOOP									= 1
#CLI_DATA_DEVICE_DOES_NOT_EXIST				= -2
#CLI_METADDATA_DEVICE_DOES_NOT_EXIST			= -3


#DZ_CLI_VERSION								= "1.1.a"
#DZ_DRIVER_VERSION							= "3.10.101"
#DZ_CLI_PRODUCT								= "AISA Volume Manager (AVM)"
#DZ_CLI_CONFIG_FILE_PATH 					= "/etc/datumsoft/avm.conf"	

#AVM_SYSFS_PARENT_PATH						= "/sys/avm/"
#AVM_SYSFS_PARENT							= "/sys/avm/"

#KERNEL_MODULE_LIST				= [
#	"datumsoft_avm_aisa_target_module", 
#   	"datumsoft_avm_volume_engine_module",
#   	"datumsoft_avm_sysfs_engine_module"
#								]

#DEVICE_TYPE_AISA				 			= "DEVICE_TYPE_AISA"
#DEVICE_TYPE_DATA_VOLUME				 		= "DEVICE_TYPE_DATA_VOLUME"
#DEVICE_TYPE_DATA_VOLUME_SNAPSHOT_READ		= "DEVICE_TYPE_DATA_VOLUME_SNAPSHOT_READ"
#DEVICE_TYPE_DATA_VOLUME_SNAPSHOT_WRITE		= "DEVICE_TYPE_DATA_VOLUME_SNAPSHOT_WRITE"
#DEVICE_TYPE_VM_VOLUME				 		= "DEVICE_TYPE_VM_VOLUME"
#DEVICE_TYPE_VM_VOLUME_SNAPSHOT_READ			= "DEVICE_TYPE_VM_VOLUME_SNAPSHOT_READ"
#DEVICE_TYPE_VM_VOLUME_SNAPSHOT_WRITE		= "DEVICE_TYPE_VM_VOLUME_SNAPSHOT_WRITE"

# Target Type Table Name inside Kernel
#TABLE_NAME_AISA					 			= "AVM_TARGET"
#TABLE_NAME_AISA_DATA_VOLUME			 		= "AVM_VOLUME"
#TABLE_NAME_AISA_DATA_VOLUME_SNAPSHOT_READ	= "AVM_VOLUME_SR"
#TABLE_NAME_AISA_DATA_VOLUME_SNAPSHOT_WRITE	= "AVM_VOLUME_SW"

#TABLE_NAME_AISA_VM_VOLUME			 		= "AVM_VOLUME_VM"
#TABLE_NAME_AISA_VM_VOLUME_SNAPSHOT_READ		= "AVM_VOLUME_VMSR"
#TABLE_NAME_AISA_VM_VOLUME_SNAPSHOT_WRITE	= "AVM_VOLUME_VMSW"


#AISA_DEVICE_HOME				 			= "/dev/mapper"
#AISA_VOLUME_PREFIX				 			= "/dev/mapper"
#AISA_DEVICE_NAME				 			= "AISA"
#AISA_DEVICE_PATH				 			= AISA_DEVICE_HOME + "/" + AISA_DEVICE_NAME
# CONFIG_AISA_DATA_DEVICE_NAME						= "/dev/md0"
# CONFIG_AISA_METADATA_DEVICE_NAME					= "/dev/md1"

#global CONFIG_AISA_DATA_DEVICE_NAME
#global CONFIG_AISA_METADATA_DEVICE_NAME
#global CONFIG_AISA_PASSTHROUGH_MODE
#global CONFIG_AISA_PASSTHROUGH_MODE_READ
#global CONFIG_AISA_PASSTHROUGH_MODE_WRITE

#CONFIG_AISA_DATA_DEVICE_NAME 						= ""
#CONFIG_AISA_METADATA_DEVICE_NAME 					= ""
#CONFIG_AISA_PASSTHROUGH_MODE	 					= ""
#CONFIG_AISA_PASSTHROUGH_MODE_READ					= ""
#CONFIG_AISA_PASSTHROUGH_MODE_WRITE					= ""
AISA_METADATA_DISK_MAGIC_NO		 			= "AISAMTDT"

# CONFIG_AISA_DATA_DEVICE_NAME	= "/dev/loop0"
# CONFIG_AISA_DATA_DEVICE_NAME	= "/dev/sdb12"

###########################################################
# AVM_CORE
# Class for creating AISA Target and Volumes.
# It interacts with the device mapper framework by 
# calling dmsetup commands.
# This class is dedicated for creating/deleting AISA target
# data volumes, virtual machine volumes and snapshot
# volumes
###########################################################

class AVM_CORE():
	'Common base class for AISA Volume Manager operations'

	def     __init__(self, arg_data_device_name, arg_metadata_device_name):
		self.data = []
		str = 'Class ' + __name__ + ' defined'
		self.data_device_name 		= arg_data_device_name
		self.metadata_device_name 	= arg_metadata_device_name

	def get_kernel_version(self):
		cmdstr	= ''
		cmdstr	+= 'cat ' + AVM_SYSFS_PARENT_PATH + 'kernel/get_version'
		cmdstr	+= ''
		retobj = ShellCmd.runr(cmdstr)
		if (retobj.retcode != 0):
			LOG(ERROR, "Unable to get kernel version. Looks like kernel module is NOT loaded")
			return CLI_FAILURE
			
		data = retobj.retstr
		verstr = data.split("\n")
		for line in verstr:
			if (line.startswith( '#' )):
				continue
			else:
				print(line)

		return CLI_SUCCESS

	def get_sysfs_entries(self):
		cmdstr	= ''
		AVM_SYSFS_PATH = AVM_SYSFS_PARENT_PATH
		print("AVM Sysfs Path : " + AVM_SYSFS_PATH)
		#print("\n")
		if os.path.exists(AVM_SYSFS_PATH):

			list_dir = sorted(os.listdir(AVM_SYSFS_PATH))

			for dir_name in list_dir:

				print(AVM_SYSFS_PATH + dir_name)
				list_files = sorted(os.listdir(AVM_SYSFS_PATH + dir_name))

				for file_name in list_files:
					print("\t" + AVM_SYSFS_PATH + dir_name + "/"+ file_name)

				#print("\n")
			return CLI_SUCCESS
		else:
			LOG (ERROR, "Path " + AVM_SYSFS_PATH + " does not exist. AVM driver in kernel is not loaded\n")
			return CLI_FAILURE

	# Setting the switch value into kernel using sysfs interface
	# Returning None object if failed
	# else return retobj
	def set_kernel_switch_tunable_value(self, switch_name, switch_value):

		AVM_SYSFS_PATH = AVM_SYSFS_PARENT_PATH
		#LOG(DEBUG,"AVM_SYSFS_PATH = " + AVM_SYSFS_PATH)
		syspath = AVM_SYSFS_PATH + switch_name
		cmdstr	= ''

		if os.path.exists(AVM_SYSFS_PATH):
			cmdstr	+= 'echo ' + str(switch_value) + ' > ' + syspath
			cmdstr	+= ''
			#print (cmdstr)
		else:
			LOG (ERROR, syspath + " does not exist. "+AVM_SYSFS_PATH)
			return None

		LOG(DEBUG, cmdstr)
		LOG(INFO, "Loading Switch Into Kernel: " + switch_name + " = " + switch_value)
		retobj = ShellCmd.runr(cmdstr)
		if (retobj.retcode != 0):
			LOG(ERROR, "Unable to load value for "+switch_name+". Check path.")
			return None
		else:
			return retobj

	# Getting the switch value from kernel using sysfs interface
	# Returning None object if failed
	# else return the value of switch/tunable present in kernel
	def get_kernel_switch_tunable_value(self, switch_name):

		AVM_SYSFS_PATH = AVM_SYSFS_PARENT_PATH
		syspath = AVM_SYSFS_PATH + switch_name
		cmdstr	= ''

		if os.path.exists(AVM_SYSFS_PATH):
			cmdstr	+= 'cat ' + syspath
			cmdstr	+= ''
			#print (cmdstr)
		else:
			LOG (ERROR, syspath + " does not exist. Check correct sysfs entries in "+AVM_SYSFS_PATH)
			return None

		LOG(INFO, "Fetching Switch Value From Kernel: " + switch_name)
		retobj = ShellCmd.runr(cmdstr)
		if (retobj.retcode != 0):
			LOG(ERROR, "Unable to set value for "+switch_name+". Looks like kernel module is NOT loaded")
			return None
		else:
			line = retobj.retstr
			tokenstr = line.split(":")
			for  token in tokenstr:
				if (token.startswith( '#' )):
					continue
				else:
					#print(Trim(token))
					return (Trim(token))

			return None

	def get_kernel_memory(self):
		cmdstr	= ''
		cmdstr	+= 'cat ' + AVM_SYSFS_PARENT_PATH + 'kernel/get_memory_info'
		cmdstr	+= ''
		retobj = ShellCmd.runr(cmdstr)
		if (retobj.retcode != 0):
			LOG(ERROR, "Unable to get kernel memory information. Looks like kernel module is NOT loaded")
			return CLI_FAILURE
			
		data = retobj.retstr
		verstr = data.split("\n")
		for line in verstr:
			if (line.startswith( '#' )):
				continue
			else:
				print(line)

		return CLI_SUCCESS

	#Get Superblock header info from kernel
	def get_kernel_superblock(self):
		cmdstr	= ''
		cmdstr	+= 'cat ' + AVM_SYSFS_PARENT_PATH + 'metadata_engine/get_superblock'
		cmdstr	+= ''
		retobj = ShellCmd.runr(cmdstr)
		if (retobj.retcode != 0):
			LOG(ERROR, "Unable to get kernel superblock information. Looks like kernel module is NOT loaded")
			return CLI_FAILURE
			
		data = retobj.retstr
		verstr = data.split("\n")
		for line in verstr:
			if (line.startswith( '#' )):
				continue
			else:
				print(line)

		return CLI_SUCCESS

	#InMemory Tables such as LBA, Hash and PBA
	def get_kernel_table_size(self):
		cmdstr	= ''
		cmdstr	+= 'cat ' + AVM_SYSFS_PARENT_PATH + 'dedupe_engine/get_table_size'
		cmdstr	+= ''
		retobj = ShellCmd.runr(cmdstr)
		if (retobj.retcode != 0):
			LOG(ERROR, "Unable to get kernel InMemory Tables Size. Looks like kernel module is NOT loaded")
			return CLI_FAILURE
			
		data = retobj.retstr
		verstr = data.split("\n")
		for line in verstr:
			if (line.startswith( '#' )):
				continue
			else:
				print(line)

		return CLI_SUCCESS

	#Metadata Engine Counters 
	def get_kernel_counters_metadata(self):

		cmdstr	= ''
		cmdstr	+= 'cat ' + AVM_SYSFS_PARENT_PATH + 'metadata_engine/get_metadata_counters'
		cmdstr	+= ''
		retobj = ShellCmd.runr(cmdstr)
		if (retobj.retcode != 0):
			LOG(ERROR, "Unable to get kernel metadata counters. Looks like kernel module is NOT loaded")
			return CLI_FAILURE
			
		data = retobj.retstr
		verstr = data.split("\n")
		for line in verstr:
			if (line.startswith( '#' )):
				continue
			else:
				print(line)

		return CLI_SUCCESS

	#FrontEnd Cache Engine Counters 
	def get_kernel_counters_fec(self):
		cmdstr	= ''
		cmdstr	+= 'cat ' + AVM_SYSFS_PARENT_PATH + 'front_end_cache_engine/get_fec_counters'
		cmdstr	+= ''
		retobj = ShellCmd.runr(cmdstr)
		if (retobj.retcode != 0):
			LOG(ERROR, "Unable to get kernel fec counters. Looks like kernel module is NOT loaded")
			return CLI_FAILURE
			
		data = retobj.retstr
		verstr = data.split("\n")
		for line in verstr:
			if (line.startswith( '#' )):
				continue
			else:
				print(line)

		return CLI_SUCCESS

	#Alignment Engine Counters 
	def get_kernel_counters_align(self):
		cmdstr	= ''
		cmdstr	+= 'cat ' + AVM_SYSFS_PARENT_PATH + 'alignment_engine/get_align_counters'
		cmdstr	+= ''
		retobj = ShellCmd.runr(cmdstr)
		if (retobj.retcode != 0):
			LOG(ERROR, "Unable to get kernel align counters. Looks like kernel module is NOT loaded")
			return CLI_FAILURE
			
		data = retobj.retstr
		verstr = data.split("\n")
		for line in verstr:
			if (line.startswith( '#' )):
				continue
			else:
				print(line)

		return CLI_SUCCESS

	#BackEnd Cache Engine Counters 
	def get_kernel_counters_bec(self):
		cmdstr	= ''
		cmdstr	+= 'cat ' + AVM_SYSFS_PARENT_PATH + 'back_end_cache_engine/get_bec_counters'
		cmdstr	+= ''
		retobj = ShellCmd.runr(cmdstr)
		if (retobj.retcode != 0):
			LOG(ERROR, "Unable to get kernel bec counters. Looks like kernel module is NOT loaded")
			return CLI_FAILURE
			
		data = retobj.retstr
		verstr = data.split("\n")
		for line in verstr:
			if (line.startswith( '#' )):
				continue
			else:
				print(line)

		return CLI_SUCCESS

	#Dedupe Engine Counters 
	def get_kernel_counters_dedupe(self):
		cmdstr	= ''
		cmdstr	+= 'cat ' + AVM_SYSFS_PARENT_PATH + 'dedupe_engine/get_dedupe_counters'
		cmdstr	+= ''
		retobj = ShellCmd.runr(cmdstr)
		if (retobj.retcode != 0):
			LOG(ERROR, "Unable to get kernel dedupe engine counters. Looks like kernel module is NOT loaded")
			return CLI_FAILURE
			
		data = retobj.retstr
		verstr = data.split("\n")
		for line in verstr:
			if (line.startswith( '#' )):
				continue
			else:
				print(line)

		return CLI_SUCCESS

	def validate_aisa_target(self, arg_show):
		if os.path.exists(AISA_DEVICE_PATH):
			if (arg_show == 1): 
				LOG (INFO, "AISA Target created. " + AISA_DEVICE_PATH + " exist")
				return CLI_SUCCESS
			else:
				return CLI_SUCCESS
		else:
			LOG (ERROR, "AISA Target Not created. " + AISA_DEVICE_PATH + " does not exist")
			return CLI_FAILURE
	
	# Note here Metadata device info is NOT passed
	def create_volume(self, arg_type, arg_name, arg_size, arg_parent):

		# If AISA target does not exist then create a default one first
		if (self.validate_aisa_target(0) < 0):
			LOG(INFO, "AISA Target does not exist. Creating default size target")
			retobj = self.create_aisa_default()
			if (retobj.retcode == 0):
				LOGR("AISA Target created")
			else:
				LOG(ERROR, "Unable to create AISA Target. Exiting...")
				return retobj.retcode

		aisa_size_blocks = self.get_aisa_size_blocks()
		if (int(arg_size) > int(aisa_size_blocks)): #{
			errstr =  "Volume Size cannot be greater than AISA Size\n"
			errstr += "Volume Size : " + arg_size + " blocks\n"
			errstr += "AISA   Size : " + aisa_size_blocks + " blocks\n"
			retobj = ReturnInfo(CLI_FAILURE, errstr)
			return retobj

		#}

		dev_start_sector 	 = 0 
		dev_len				 = arg_size

		if arg_type	 == DEVICE_TYPE_DATA_VOLUME:
			dev_table_name	= TABLE_NAME_AISA_DATA_VOLUME
			dev_parent 		= ''
			dev_magic_str	= 'VOLUME'		

		if arg_type	 == DEVICE_TYPE_DATA_VOLUME_SNAPSHOT_READ:
			dev_table_name		 = TABLE_NAME_AISA_DATA_VOLUME_SNAPSHOT_READ
			# dev_parent			= AISA_DEVICE_HOME + "/" + arg_parent
			dev_parent			 = arg_parent
			dev_magic_str	= 'VOLUSR'		

		if arg_type	 == DEVICE_TYPE_DATA_VOLUME_SNAPSHOT_WRITE:
			dev_table_name		 = TABLE_NAME_AISA_DATA_VOLUME_SNAPSHOT_WRITE
			# dev_parent			= AISA_DEVICE_HOME + "/" + arg_parent
			dev_parent			 = arg_parent
			dev_magic_str	= 'VOLUSW'		
		
		# Note Data device will point to AISA device name, because
		# volumes are carved out on AISA Device
		dev_data_device_name = AISA_DEVICE_PATH  # argv[0]
		dev_start_sector_2	 = 0  # argv[1]
		dev_type			 = arg_type  # argv[2] 
		dev_name			 = arg_name  # argv[3] Name of current volume
		dev_magic			 = self.get_magic_no(dev_magic_str)  # argv[4]
		retobj				 = getUUID()  # argv[5]
		dev_uuid			 = retobj.retstr

		cmdstr = ''
		cmdstr += 'dmsetup create ' + dev_name
		cmdstr += ' --uuid ' 		 + str(dev_uuid)
		cmdstr += ' --table "'
		cmdstr += ' ' + str(dev_start_sector)	
		cmdstr += ' ' + str(dev_len)						
		cmdstr += ' ' + dev_table_name						
		cmdstr += ' ' + dev_data_device_name  # argv[0]						
		cmdstr += ' ' + str(dev_start_sector_2)  # argv[1]
		cmdstr += ' ' + dev_type  # argv[2]
		cmdstr += ' ' + dev_name  # argv[3] Passed twice
		cmdstr += ' ' + dev_magic  # argv[4]
		cmdstr += ' ' + dev_uuid  # argv[5]
		cmdstr += ' ' + dev_parent  # argv[6]
		cmdstr += ' ' + getCurrentDateRaw() + getCurrentTimeRaw()# argv[7] # Time in "ddmmyyyyhhmmss"
		cmdstr += ' "'
		#LOG(INFO, "MagicNo : " + dev_magic)
		#LOG(INFO, "UUID    : " + dev_uuid)
		return ShellCmd.runo(cmdstr)

	def get_aisa_size_blocks(self):
		return self.get_device_size_blocks(AISA_DEVICE_PATH)

	def get_aisa_size_bytes(self):
		return self.get_device_size_bytes(AISA_DEVICE_PATH)

	def get_aisa_default_size_blocks(self):
		#default size will be the size of data device
		#return self.get_device_size_blocks(CONFIG_AISA_DATA_DEVICE_NAME)
		return self.get_device_size_blocks(self.data_device_name)

	def get_aisa_default_size_bytes(self):
		#default size will be the size of data device
		#return self.get_device_size_bytes(CONFIG_AISA_DATA_DEVICE_NAME)
		return self.get_device_size_bytes(self.data_device_name)

	def create_aisa_default(self):
		dev_type 	= DEVICE_TYPE_AISA
		dev_name	= AISA_DEVICE_NAME

		#By default create the AISA target with the size of data device
		dev_size_blocks = self.get_aisa_default_size_blocks()
		if (int(dev_size_blocks) <= 0 ):
			#errstr =  "Data Device "+ CONFIG_AISA_DATA_DEVICE_NAME
			errstr =  "Data Device "+ self.data_device_name
			errstr += " does not exist or not accessible"

			#LOG(ERROR,"Data Device "+ CONFIG_AISA_DATA_DEVICE_NAME)
			LOG(ERROR,"Data Device "+ self.data_device_name)
			LOGRAW(" does not exist or not accessible")

			retobj = ReturnInfo(CLI_FAILURE, errstr)
			return retobj
		else:
			LOG(INFO,"Creating AISA Target of default size " + dev_size_blocks + " blocks")

		return (self.create_aisa(dev_type, dev_name, dev_size_blocks))

	# Note here Metadata device info is also passed
	def create_aisa(self, arg_type, arg_name, arg_size):

		dev_start_sector 	 = 0 
		dev_len				 = arg_size
		dev_table_name		 = TABLE_NAME_AISA
		# Note Data device will point to AISA DATA DEVICE
		#dev_data_device_name = CONFIG_AISA_DATA_DEVICE_NAME  # argv[0]
		dev_data_device_name = self.data_device_name  # argv[0]
		dev_start_sector_2	 = 0  # argv[1]
		dev_type			 = arg_type  # argv[2] 
		dev_name			 = arg_name  # argv[3]
		dev_magic_str		 = 'AISADD'	 # AISA Data Device		
		dev_magic			 = self.get_magic_no(dev_magic_str)  # argv[4]
		retobj				 = getUUID()  # argv[5]
		dev_uuid			 = retobj.retstr

		#dev_metadata_name	 = CONFIG_AISA_METADATA_DEVICE_NAME
		dev_metadata_name	 = self.metadata_device_name
		dev_magic_str		 = 'AISAMD'	 # AISA MetaData Device	
		dev_metadata_magic	 = self.get_magic_no(dev_magic_str)
		retobj				 = getUUID()
		dev_metadata_uuid	 = retobj.retstr

		cmdstr = ''
		cmdstr += 'dmsetup create ' + dev_name
		cmdstr += ' --uuid ' 		 + str(dev_uuid)
		cmdstr += ' --table "'
		cmdstr += ' ' + str(dev_start_sector)	
		cmdstr += ' ' + str(dev_len)						
		cmdstr += ' ' + dev_table_name						
		cmdstr += ' ' + dev_data_device_name  # argv[0]						
		cmdstr += ' ' + str(dev_start_sector_2)  # argv[1]
		cmdstr += ' ' + dev_type  # argv[2]
		cmdstr += ' ' + dev_name  # argv[3] Passed twice
		cmdstr += ' ' + dev_magic  # argv[4]
		cmdstr += ' ' + dev_uuid  # argv[5]
		cmdstr += ' ' + dev_metadata_name  # argv[6]
		cmdstr += ' ' + dev_metadata_magic  # argv[7]
		cmdstr += ' ' + dev_metadata_uuid  # argv[8]
		cmdstr += ' ' + getCurrentDateRaw() + getCurrentTimeRaw()# argv[9] # Time in "ddmmyyyyhhmmss"
		cmdstr += ' "'
		#LOG(INFO, "MagicNo : " + dev_magic)
		#LOG(INFO, "UUID    : " + dev_uuid)
		return ShellCmd.runo(cmdstr)

	def delete_volume(self, arg_devname):
		dev_name	= arg_devname
		cmdstr = ''
		cmdstr += 'dmsetup remove ' + dev_name
		cmdstr += ''
		LOG(INFO, "Deleting Volume : " + dev_name)
		return ShellCmd.runo(cmdstr)

	def delete_all_volumes(self):
		retobj = self.list_volume_short()
		data = retobj.retstr
		devices = data.split("\n")
		for vol in devices:
			if (vol.startswith( '#' )):
				continue
			else:
				retobj = self.delete_volume(vol)
				retobj.LOG()

	def delete_aisa(self):
		self.delete_all_volumes()
		dev_name	= "AISA"
		cmdstr = ''
		cmdstr += 'dmsetup remove ' + dev_name
		cmdstr += ''
		LOG(INFO, "Deleting AISA Target : " + dev_name)
		return ShellCmd.runo(cmdstr)

	def kernel_sizeof_datastructures(self, argc, argv):
		cmdstr	= ''
		cmdstr	+= 'cat ' + AVM_SYSFS_PARENT_PATH + 'kernel/get_sizeof_datastructures'
		cmdstr	+= ''
		return ShellCmd.runr(cmdstr)

	def kernel_io_read(self, argc, argv):
		device_name = argv[2]
		sector 		= argv[3]
		size 		= argv[4]
		verbose		= ""
		if (argc == 6):
			verbose 	= argv[5]

		LOG(INFO, "READ IO Details")
		LOG(INFO, "===============")
		LOG(INFO, "DEVICE NAME   : " + device_name)
		LOG(INFO, "DEVICE LBA    : " + sector)
		LOG(INFO, "DEVICE SIZE   : " + size)
		cmdstr	= ''
		cmdstr	+= 'echo ' + device_name + ' > ' + AVM_SYSFS_PARENT_PATH + 'target_engine/issue_read_io_on_device;\n'
		cmdstr	+= 'echo ' + sector +      ' > ' + AVM_SYSFS_PARENT_PATH + 'target_engine/issue_read_io_at_sector;\n'
		cmdstr	+= 'echo ' + size +        ' > ' + AVM_SYSFS_PARENT_PATH + 'target_engine/issue_read_io_with_size;\n'
		cmdstr	+= 'cat ' +                ' ' + AVM_SYSFS_PARENT_PATH + 'target_engine/issue_read_io;\n'
		cmdstr	+= ''

		if (verbose == "-v"):
			LOG(INFO, "Verbose:Command Details")
			LOG(INFO, "=======================")
			#LOG(INFO, "\n" + cmdstr)
			print(cmdstr)

		return ShellCmd.runr(cmdstr)

	def kernel_io_write(self, argc, argv):
		device_name = argv[2]
		sector 		= argv[3]
		size 		= argv[4]
		verbose		= ""
		if (argc == 6):
			verbose 	= argv[5]
		LOG(INFO, "WRITE IO Details")
		LOG(INFO, "================")
		LOG(INFO, "DEVICE NAME    : " + device_name)
		LOG(INFO, "DEVICE LBA     : " + sector)
		LOG(INFO, "DEVICE SIZE    : " + size)
		cmdstr	= ''
		cmdstr	+= 'echo ' + device_name + ' > ' + AVM_SYSFS_PARENT_PATH + 'target_engine/issue_write_io_on_device;\n'
		cmdstr	+= 'echo ' + sector +      ' > ' + AVM_SYSFS_PARENT_PATH + 'target_engine/issue_write_io_at_sector;\n'
		cmdstr	+= 'echo ' + size +        ' > ' + AVM_SYSFS_PARENT_PATH + 'target_engine/issue_write_io_with_size;\n'
		cmdstr	+= 'cat ' +                ' ' + AVM_SYSFS_PARENT_PATH + 'target_engine/issue_write_io\n'
		cmdstr	+= ''

		if (verbose == "-v"):
			LOG(INFO, "Verbose:Command Details")
			LOG(INFO, "=======================")
			#LOG(INFO, "\n" + cmdstr)
			print(cmdstr)

		return ShellCmd.runr(cmdstr)

	def kernel_io_zero(self, argc, argv):
		device_name = argv[2]
		sector 		= argv[3]
		size 		= argv[4]
		verbose		= ""
		if (argc == 6):
			verbose 	= argv[5]
		LOG(INFO, "ZERO IO Details")
		LOG(INFO, "================")
		LOG(INFO, "DEVICE NAME    : " + device_name)
		LOG(INFO, "DEVICE LBA     : " + sector)
		LOG(INFO, "DEVICE SIZE    : " + size)
		cmdstr	= ''
		cmdstr	+= 'echo ' + device_name + ' > ' + AVM_SYSFS_PARENT_PATH + 'target_engine/issue_zero_io_on_device;\n'
		cmdstr	+= 'echo ' + sector +      ' > ' + AVM_SYSFS_PARENT_PATH + 'target_engine/issue_zero_io_at_sector;\n'
		cmdstr	+= 'echo ' + size +        ' > ' + AVM_SYSFS_PARENT_PATH + 'target_engine/issue_zero_io_with_size;\n'
		cmdstr	+= 'cat ' +                ' ' + AVM_SYSFS_PARENT_PATH + 'target_engine/issue_zero_io\n'
		cmdstr	+= ''

		if (verbose == "-v"):
			LOG(INFO, "Verbose:Command Details")
			LOG(INFO, "=======================")
			#LOG(INFO, "\n" + cmdstr)
			print(cmdstr)

		return ShellCmd.runr(cmdstr)



	def list_volume(self, argc, argv):

		cmdstr	= ''
		cmdstr  += 'cat ' 
		cmdstr += AVM_SYSFS_PARENT_PATH + 'volume_engine/get_list_of_volumes ;'
		cmdstr	+= ''
		return ShellCmd.runr(cmdstr)

	def list_volume_short(self):

		cmdstr  = ''
		cmdstr  += 'cat ' 
		cmdstr += AVM_SYSFS_PARENT_PATH + 'volume_engine/get_list_of_volumes_short ;'
		cmdstr  += ''
		return ShellCmd.runr(cmdstr)

	def info_aisa(self):
		cmdstr = ''
		cmdstr += 'echo "' + AISA_DEVICE_NAME + '"> '
		cmdstr += AVM_SYSFS_PARENT_PATH + 'volume_engine/get_set_volume_info ;'

		cmdstr += 'cat '
		cmdstr += AVM_SYSFS_PARENT_PATH + 'volume_engine/get_set_volume_info ;'
		cmdstr += ''
		return ShellCmd.runr(cmdstr)

	def info_volume(self, arg_name):
		cmdstr = ''
		cmdstr += 'echo "'+arg_name+'"> '
		cmdstr += AVM_SYSFS_PARENT_PATH + 'volume_engine/get_set_volume_info ;'

		cmdstr += 'cat '
		cmdstr += AVM_SYSFS_PARENT_PATH + 'volume_engine/get_set_volume_info ;'
		cmdstr += ''
		return ShellCmd.runr(cmdstr)

	def get_kernel_lba_entry(self, arg_lba_index):

		cmdstr = ''
		cmdstr += 'echo "' + arg_lba_index + '"> '
		cmdstr += AVM_SYSFS_PARENT_PATH + 'dedupe_engine/get_set_lba_table_index ;'

		cmdstr += 'cat '
		cmdstr += AVM_SYSFS_PARENT_PATH + 'dedupe_engine/get_set_lba_table_index ;'
		cmdstr += ''

		retobj = ShellCmd.runr(cmdstr)
		if (retobj.retcode != 0):
			LOG(ERROR, "Unable to get kernel lba table entry. Looks like kernel module is NOT loaded")
			return CLI_FAILURE
			
		data = retobj.retstr
		verstr = data.split("\n")
		for line in verstr:
			if (line.startswith( '#' )):
				continue
			else:
				print(line)

		return CLI_SUCCESS

	def get_kernel_pba_entry(self, arg_pba_index):
		cmdstr = ''
		cmdstr += 'echo "' + arg_pba_index + '"> '
		cmdstr += AVM_SYSFS_PARENT_PATH + 'dedupe_engine/get_set_pba_table_index ;'

		cmdstr += 'cat '
		cmdstr += AVM_SYSFS_PARENT_PATH + 'dedupe_engine/get_set_pba_table_index ;'
		cmdstr += ''

		retobj = ShellCmd.runr(cmdstr)
		if (retobj.retcode != 0):
			LOG(ERROR, "Unable to get kernel pba table entry. Looks like kernel module is NOT loaded")
			return CLI_FAILURE
			
		data = retobj.retstr
		verstr = data.split("\n")
		for line in verstr:
			if (line.startswith( '#' )):
				continue
			else:
				print(line)

		return CLI_SUCCESS

	def get_device_size_bytes(self, arg_volname):
		#volname = AISA_VOLUME_PREFIX + "/" + arg_volname
		volname = arg_volname
		cmdstr = ''
		cmdstr += 'echo "'+volname+'"> '
		cmdstr += AVM_SYSFS_PARENT_PATH + 'target_engine/get_target_device_size_bytes ;'

		cmdstr += 'cat '
		cmdstr += AVM_SYSFS_PARENT_PATH + 'target_engine/get_target_device_size_bytes ;'
		cmdstr += ''
		retobj = ShellCmd.runr(cmdstr)

		output = retobj.retstr
		size_in_bytes = output.split("\n")
		for sz in size_in_bytes:
			if (sz.startswith( '#' )):
				continue
			else:
				#LOGR("Size in Bytes "+sz)
				return sz

		return str(-1)

	def get_device_size_blocks(self, arg_volname):
		#volname = AISA_VOLUME_PREFIX + "/" + arg_volname
		volname = arg_volname
		cmdstr = ''
		cmdstr += 'echo "'+volname+'"> '
		cmdstr += AVM_SYSFS_PARENT_PATH + 'target_engine/get_target_device_size_blocks ;'

		cmdstr += 'cat '
		cmdstr += AVM_SYSFS_PARENT_PATH + 'target_engine/get_target_device_size_blocks ;'
		cmdstr += ''
		retobj = ShellCmd.runr(cmdstr)

		output = retobj.retstr
		size_in_blocks = output.split("\n")
		for sz in size_in_blocks:
			if (sz.startswith( '#' )):
				continue
			else:
				#LOGR("Size in Blocks "+sz)
				return sz

		return str(-1)

	def print_volume_size_all(self):
		retobj = self.list_volume_short()
		data = retobj.retstr
		devices = data.split("\n")
		for vol in devices:
			if (vol.startswith( '#' )):
				continue
			else:
				volpath = AISA_VOLUME_PREFIX + "/" + vol
				dev_size_blocks = self.get_device_size_blocks(volpath)
				dev_size_bytes  = self.get_device_size_bytes(volpath)
				LOGRAW("[ " + volpath + " ]")
				LOGRAW("  " + volpath +" size in Bytes : " + dev_size_bytes)
				LOGRAW("  " + volpath +" size in Blocks: " + dev_size_blocks)
				LOGRAW("  " + volpath +" size in KB    : " + convert_bytes_to_kb(dev_size_bytes))
				LOGRAW("  " + volpath +" size in MB    : " + convert_bytes_to_mb(dev_size_bytes))
				LOGRAW("  " + volpath +" size in GB    : " + convert_bytes_to_gb(dev_size_bytes))

			LOGRAW("")

	def print_aisa_size(self):
		volpath = AISA_DEVICE_PATH
		dev_size_blocks = self.get_device_size_blocks(volpath)
		dev_size_bytes  = self.get_device_size_bytes(volpath)
		LOGRAW("[ " + volpath + " ]")
		LOGRAW("  " + volpath +" size in Bytes : " + dev_size_bytes)
		LOGRAW("  " + volpath +" size in Blocks: " + dev_size_blocks)
		LOGRAW("  " + volpath +" size in KB    : " + convert_bytes_to_kb(dev_size_bytes))
		LOGRAW("  " + volpath +" size in MB    : " + convert_bytes_to_mb(dev_size_bytes))
		LOGRAW("  " + volpath +" size in GB    : " + convert_bytes_to_gb(dev_size_bytes))
		LOGRAW("")

	def get_magic_no(self, argstr):
		magicstr = ""
		for ch in argstr: 
			magicstr += getHex(ch)

		return magicstr
