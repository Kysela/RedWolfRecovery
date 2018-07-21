 /*
	Copyright 2018 ATG Droid/Dadi11 RedWolf
	This file is part of RWRP/RedWolf Recovery Project.

	RWRP is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	RWRP is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with RWRP.  If not, see <http://www.gnu.org/licenses/>.
*/

// dumwolf.cpp - Source to unpack/repack boot & recovery images

#include <stdio.h>
#include <algorithm>
#include <sys/stat.h>
#include <dirent.h>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include "twrp-functions.hpp"
#include "dumwolf.hpp"
#include "data.hpp"
#include "partitions.hpp"
#include "twcommon.h"
#include "cutils/properties.h"
#include "gui/gui.hpp"
#include "variables.h"

extern "C" {
	#include "libcrecovery/common.h"
}

using str_t = std::string;



static const str_t tmp = "/tmp/dumwolf";
static const str_t ramdisk = tmp + "/ramdisk";
static const str_t split_img = tmp + "/split_img";

str_t RWDumwolf::Find_Ramdisk(void) {
str_t filename;
size_t pos;
DIR *d;
struct dirent *p;
d = opendir(split_img.c_str());
if (d == NULL)
{
LOGINFO("Dumwolf: Unable to open '%s'\n", split_img.c_str());
return "";
}
while ((p = readdir(d)) != NULL) {
filename = p->d_name;
if (filename == "." || filename == "..")
continue;
pos = filename.find_last_of("-") + 1;
if (pos == str_t::npos) continue;
if (filename.substr(pos) == "ramdisk.gz")
break;
}
closedir (d);
if (!filename.empty())
return (split_img + "/" + filename);
else
return "";
}

bool RWDumwolf::Execute(const str_t& exec) {
return (0 == __system(exec.c_str()));
}

bool RWDumwolf::Is_Ramdisk_Unpacked(void) { 
    DIR* d;
    uint8_t i = 0;
    struct dirent* de;
    d = opendir(split_img.c_str());
    
    if (d == NULL)
    {
       LOGINFO("Dumwolf: Unable to open '%s'\n", ramdisk.c_str());
      return false;
    }
       while ((de = readdir(d)) != NULL)
       {
             i++;
             if (i > 4)
             break;
        }
            closedir (d);
            return (i > 4);
     }

Ramdisk_Compression RWDumwolf::Get_Ramdisk_Compression(const str_t& filename) {
	 if (filename.empty())
	 return UNKNOWN;

	FILE *archive = fopen(filename.c_str(), "rb");
    if (archive == NULL) {
        LOGINFO("Dumwolf: Failed to open archive: %s\n", filename.c_str());
        return UNKNOWN;
    }
    
    char buff[3];
    fread(buff, 1, 2, archive);
    
    if (fclose(archive) != NULL) {
        LOGERR("Dumwolf: Failed to close archive: '%s'\n", filename.c_str());
        return UNKNOWN;
    }    
    uint8_t i;
    const char bytes[12] = {0x1f, 0x8b,0x02, 0x21,0x42, 0x5a, 0xfd, 0x37, 0x89, 0x4c, 0x5d, 0x00};
   
 for (i=0;i<sizeof(bytes);i+2) {
   if (buff[0] == bytes[i] && buff[1] == bytes[i+1]) {
 switch (i) {
   case GZIP:
      return GZIP;
   case LZ4:
      return LZ4;
   case BZIP2:
      return BZIP2;
   case XZ:
      return XZ;
   case LZOP:
      return LZOP;
   case LZMA:
      return LZMA;
   default:
      break;
    }
   }
 }
 LOGINFO("Dumwolf: Unable to detect compression format of '%s', extracted bytes: '%s/%s'\n", filename.c_str(), buff[0], buff[1]);
 return UNKNOWN;
}


void RWDumwolf::Remove_Start_Of_Service(const str_t& file_path, const str_t service) {
  str_t contents_of_file;
  const str_t renamed = file_path + ".redwolf";
  if (rename(file_path.c_str(), renamed.c_str()) != 0)
  return;
  ifstream old_file(renamed.c_str());
  ofstream new_file(file_path.c_str()); 
  while (getline(old_file, contents_of_file)) {
  if (contents_of_file.find(service) != str_t::npos)
       continue;
       else
       new_file << contents_of_file << '\n';
   }
   
  unlink(renamed.c_str());
  chmod(file_path.c_str(), 0750);  
}

bool RWDumwolf::Check_Double_String(const str_t& filename, const str_t first, const str_t second, bool both) {
    str_t line;
    ifstream File;
    File.open (filename);
    if(File.is_open()) {
       while (getline(File,line)) {
     if (both) {
      if (line.find(first) != str_t::npos && line.find(second) != str_t::npos) {
             File.close();
             return true;
             }
            } else {
           if (line.find(first) != str_t::npos || line.find(second) != str_t::npos) {
             File.close();
             return true;
             }
           }
        }
        File.close();
    }
    return false;
}


bool RWDumwolf::Set_New_Ramdisk_Property(const str_t& default_prop, const str_t prop) {
	   if (TWFunc::CheckWord(default_prop, prop)) return false;
        const str_t var = prop.substr(0, prop.find('=') + 1);
       if (TWFunc::CheckWord(default_prop, var)) {
            str_t contents_of_file;
            const str_t renamed = default_prop + ".redwolf";
       if (rename(default_prop.c_str(), renamed.c_str()) != 0)
            return false;
          ifstream old_file(renamed.c_str());
          ofstream new_file(default_prop.c_str());
        while (getline(old_file, contents_of_file)) {
        new_file << (contents_of_file.find(var) != str_t::npos ? prop : contents_of_file) << '\n';   
   }
  unlink(renamed.c_str());
  chmod(default_prop.c_str(), 0750);  
} else {
ofstream File(default_prop, ios_base::app | ios_base::out);  
if (File.is_open()) {
File << prop << endl;
File.close();
}
}
return true;
}

str_t RWDumwolf::Load_File(const str_t& extension) {
      str_t line;
      const str_t path = split_img + "/" + extension;  
      ifstream File;
      File.open (path);
      if(File.is_open()) {
       getline(File,line);
       File.close();
       }
       return line;
     }
  
bool RWDumwolf::Unpack_Image(const str_t mount_point) {
          TWPartition* Partition = PartitionManager.Find_Partition_By_Path(mount_point);
          if (Partition == NULL || !Partition->Dumwolf_Allow_Resize)
          return false;
        if (TWFunc::Path_Exists(tmp + "/."))
       TWFunc::removeDir(tmp, true);
       else
       mkdir(tmp.c_str(), 0755);
       mkdir(ramdisk.c_str(), 0755);
       mkdir(split_img.c_str(), 0755);
       
       if (!PartitionManager.Raw_Read_Write_By_Path(tmp + "/boot.img", mount_point, true)) {
        TWFunc::removeDir(tmp, false);
        return false;
        }
        
     if (!Execute("unpackbootimg -i " + tmp + "/boot.img" + " -o " + split_img)) {
        TWFunc::removeDir(tmp, false);
        return false;
        }
    
       str_t local;
       const str_t& filename = Find_Ramdisk();
    
       if (filename.empty()) {
       LOGINFO("Dumwolf: Unable to find ramdisk!\n");
       TWFunc::removeDir(tmp, false);
       return false;
    }

 switch (Get_Ramdisk_Compression(filename)) {
   case UNKNOWN:
      TWFunc::removeDir(tmp, false);
      return false;
   case GZIP:
      local = "gzip";
      break;
   case LZ4:
      local = "lz4";
      break;
   case BZIP2:
      local = "bzip2";
      break;
   case XZ:
      local = "xz";
      break;
   case LZOP:
      local = "lzop";
      break;
   case LZMA:
      local = "lzma";
      break;
   }
         LOGINFO("Dumwolf: Detected '%s' compression format.\n", local.c_str());
         local += (local == "lz4" ? " -d" : " -dc");
     if (!Execute("cd " + ramdisk + "; " + local + " < " + filename + " | cpio -i") && !Is_Ramdisk_Unpacked()) {
         LOGINFO("Dumwolf: Unable to unpack '%s'\n", mount_point.c_str());
         TWFunc::removeDir(tmp, false);
         return false;
         }
         return true;
}

void RWDumwolf::Handle_Boot_File_Type(const str_t& input, const str_t& local, str_t &output)
{
if (input == "zImage")
output += " --kernel " + split_img + "/" + local;
else if (input == "ramdisk.gz") {
output += " --ramdisk " + (TWFunc::Path_Exists(tmp + "/ramdisk-new") ? tmp + "/ramdisk-new" : split_img + "/" + local);
} else if (input == "dtb")
output += " --dt " + split_img + "/" + local;
else if (input == "secondoff")
output += " --second_offset " + Load_File(local);
else if (input == "second")
output += " --" + input + " " + split_img + "/" + local;
else if (input == "cmdline")
output += " --" + input + " \"" + Load_File(local) + "\"";
else if (input == "board")
output += " --" + input + " \"" + Load_File(local) + "\"";
else if (input == "base")
output += " --" + input + " " + Load_File(local);
else if (input == "pagesize")
output += " --" + input + " " + Load_File(local);
else if (input == "kerneloff")
output += " --kernel_offset " + Load_File(local);
else if (input == "ramdiskoff")
output += " --ramdisk_offset " + Load_File(local);
else if (input == "tagsoff")
output += " --tags_offset \"" + Load_File(local) + "\"";
else if (input == "hash") {
str_t hash = Load_File(local);
if (hash == "unknown") hash = "sha1";
output += " --" + input + " " + hash;
} else if (input == "osversion")
output += " --os_version \"" + Load_File(local) + "\"";
else if (input == "oslevel")
output += " --os_patch_level \"" + Load_File(local) + "\"";
else
LOGINFO("Dumwolf: Requested unknown boot file type: '%s'\n", input.c_str());
return;
}

bool RWDumwolf::Resize_By_Path(const str_t& path) {
str_t local, Command, oldfile;
size_t pos;
if (TWFunc::Path_Exists(tmp + "/."))
TWFunc::removeDir(tmp, true);
else
mkdir(tmp.c_str(), 0755);
mkdir(split_img.c_str(), 0755);
if (!Execute("unpackbootimg -i " + path + " -o " + split_img)) {
TWFunc::removeDir(tmp, false);
return false;
}
DIR* d;
struct dirent* de;
d = opendir(split_img.c_str());
if (d == NULL)
{
LOGINFO("Dumwolf: Unable to open '%s'\n", split_img.c_str());
TWFunc::removeDir(tmp, false);
return false;
}
Command = "mkbootimg";
while ((de = readdir(d)) != NULL)
{
local = de->d_name;
if (local == "." || local == "..")
continue;
pos = local.find_last_of("-") + 1;
oldfile = local.substr(pos);
Handle_Boot_File_Type(oldfile, local, Command);
}
closedir (d);
Command += " --output " + path;
if (!Execute(Command)) {
TWFunc::removeDir(tmp, false);
return false;
}
char brand[PROPERTY_VALUE_MAX];
property_get("ro.product.manufacturer", brand, "");
Command = brand;
if (!Command.empty()) {
transform(Command.begin(), Command.end(), Command.begin(), ::tolower);
if (Command == "samsung") {
ofstream File(path, ios_base::app | ios_base::out);
	if (File.is_open()) {
		File << "SEANDROIDENFORCE" << endl;
		File.close();
	}
 }
   }
TWFunc::removeDir(tmp, false);
return true;
}
  


bool RWDumwolf::Repack_Image(const str_t mount_point) {
str_t Command, local, oldfile;
switch (Get_Ramdisk_Compression(Find_Ramdisk())) {
  case UNKNOWN:
  TWFunc::removeDir(tmp, false);
  return false;
  case GZIP:
  Command = "gzip -9c";
  break;
  case LZ4:
  Command = "lz4 -9";
  break;
  case BZIP2:
  Command = "bzip2 -9c";
  break;
  case XZ:
  Command = "xz --check=crc32 --lzma2=dict=2MiB";
  break;
  case LZOP:
  Command = "lzop -9c";
  break;
  case LZMA:
  Command = "lzma -c";
  break;
}
Execute("cd " + ramdisk + "; find | cpio -o -H newc | " + Command + " > " + tmp + "/ramdisk-new");
DIR* d;
struct dirent* de;
size_t pos;
d = opendir(split_img.c_str());
if (d == NULL)
{
LOGINFO("Dumwolf: Unable to open '%s'\n", split_img.c_str());
TWFunc::removeDir(tmp, false);
return false;
}
Command = "mkbootimg";
while ((de = readdir(d)) != NULL)
{
local = de->d_name;
if (local == "." || local == "..")
continue;
pos = local.find_last_of("-") + 1;
oldfile = local.substr(pos);
Handle_Boot_File_Type(oldfile, local, Command);
}
closedir (d);
local = tmp + "/boot.img";
Command += " --output " + local;
oldfile = local + ".bak";
rename(local.c_str(), oldfile.c_str());
if (!Execute(Command)) {
TWFunc::removeDir(tmp, false);
return false;
}
char brand[PROPERTY_VALUE_MAX];
property_get("ro.product.manufacturer", brand, "");
Command = brand;
if (!Command.empty()) {
transform(Command.begin(), Command.end(), Command.begin(), ::tolower);
if (Command == "samsung") {
ofstream File(local, ios_base::app | ios_base::out);
	if (File.is_open()) {
		File << "SEANDROIDENFORCE" << endl;
		File.close();
	}
 }
   }
unsigned long long Remain, Remain_old;
if (TWFunc::Path_Exists(oldfile)) {
    Remain_old = TWFunc::Get_File_Size(oldfile);
    Remain = TWFunc::Get_File_Size(local);
    if (Remain_old < Remain) {
    LOGINFO("Dumwolf: File is too large for partition\n");
    TWFunc::removeDir(tmp, false);
    return false;
    }
   }
PartitionManager.Raw_Read_Write_By_Path(local, mount_point, false);
TWFunc::removeDir(tmp, false);
return true;
}

bool RWDumwolf::Patch_Encryption(const str_t& path, const str_t& fstab) {
struct stat st;
if (stat(path.c_str(), &st) == 0) {
if (Check_Double_String(path, "forceencrypt", "forcefdeorfbe", false)) {
TWFunc::Replace_Word_In_File(path, "forcefdeorfbe=;forceencrypt=;", "encryptable=");
return true;
    }
   }
return false;
}

void RWDumwolf::Deactivation_Process(void) {
time_t start, stop;
time(&start);
TWFunc::SetPerformanceMode(true);
if (!Unpack_Image("/boot")) {
LOGINFO("Dumwolf: Unable to unpack image\n");
TWFunc::SetPerformanceMode(false);
return;
}
gui_msg(Msg(msg::kProcess, "wolf_run_process=Starting '{1}' process")("Dumwolf"));
bool verity = false, encrypt = false, prop = false, treble = false;
char hardware[PROPERTY_VALUE_MAX];
property_get("ro.hardware", hardware, "error");
int dm_verity, encryption, properties = 0;
DataManager::GetValue(RW_DISABLE_DM_VERITY, dm_verity);
DataManager::GetValue(RW_DISABLE_FORCED_ENCRYPTION, encryption);
str_t path, cmp, redwolf = "fstab.";
redwolf += hardware;
DIR* d;
struct dirent* de;
d = opendir(ramdisk.c_str());
if (d == NULL)
{
LOGINFO("Dumwolf: Unable to open '%s'\n", ramdisk.c_str());
return;
}
while ((de = readdir(d)) != NULL)
{
   if (de->d_name[0] != 'f' && de->d_name[0] != 'i' && de->d_name[0] != 'd' && de->d_name[0] != 'v' && de->d_name[0] != 's') continue;
   if (de->d_name[0] != 's' && de->d_type == DT_DIR) continue;
   cmp = de->d_name;
   path = ramdisk + "/" + cmp;
   if (cmp== redwolf) {
  if (!treble) treble = true;
  gui_msg(Msg("wolf_dumwolf_fstab=Detected fstab: '{1}'")(cmp));
 if (dm_verity == 1) {
 if (Check_Double_String(path, "verify", "support_scfs", false)) {
TWFunc::Replace_Word_In_File(path, "verify,;,verify;verify;support_scfs,;,support_scfs;support_scfs;");
if (!verity) verity = true;
  }
  }
  if (encryption == 1) {
 if (Check_Double_String(path, "forceencrypt", "forcefdeorfbe", false)) {
 if (!encrypt) 
 encrypt = true;
 TWFunc::Replace_Word_In_File(path, "forcefdeorfbe=;forceencrypt=;", "encryptable=");
 }
 }
   continue;
 }
 if (de->d_name[0] == 'i') {
 if (cmp.size() > 3 && cmp.substr(strlen(de->d_name) - 3) != ".rc")
	continue;
    if (Check_Double_String(path, "service flash_recovery", "install-recovery.sh", true)) {
         LOGINFO("Dumwolf: Detected flash_recovery service under '%s'\n", cmp.c_str());
	     Remove_Start_Of_Service(path, "start flash_recovery");
  }
  continue;
}
      if (cmp == "verity_key") {
      	if (!verity)
              verity = true;
		unlink(path.c_str());
		continue;
	}
	if (cmp == "default.prop") {
	prop = true;
	continue;
	}
	if (cmp == "sbin" && de->d_type == DT_DIR) {
	path = ramdisk + "/" + cmp + "/firmware_key.cer";
    if (TWFunc::Path_Exists(path))
     unlink(path.c_str());
     }
} 
    closedir (d);
    bool vendor_state = true;
   if (!treble && encryption == 1) {
   do {
   TWPartition* Partition = PartitionManager.Find_Partition_By_Path("/vendor");
   if (Partition == NULL)
        break;
        vendor_state = PartitionManager.Is_Mounted_By_Path("/vendor");
       if (!vendor_state && !PartitionManager.Mount_By_Path("/vendor", false))
       goto system;
       path = "/vendor/etc/" + redwolf;
       if (Patch_Encryption(path, redwolf)) {
       if (!encrypt)
       encrypt = true;
       }
   system:
   path = "/system/etc/" + redwolf;
   Partition = PartitionManager.Find_Partition_By_Path("/system");
   if (Partition == NULL)
        break;
        bool mount_state = PartitionManager.Is_Mounted_By_Path("/system");
  if (!mount_state && !PartitionManager.Mount_By_Path("/system", false))
       break;
    if (Patch_Encryption(path, redwolf)) {
       if (!encrypt) 
       encrypt = true;
       }
       path = "/system/vendor/etc" + redwolf;
   if (Patch_Encryption(path, redwolf)) {
       if (!encrypt) 
       encrypt = true;
       }
   if (!mount_state)
    PartitionManager.UnMount_By_Path("/system", false);
   break;
    } while (0);
    if (encrypt)
       gui_msg(Msg("wolf_dumwolf_fstab=Detected fstab: '{1}'")(redwolf));
  }   
  
  if (prop) {
  path = ramdisk + "/default.prop";
  } else {
  TWPartition* Partition = PartitionManager.Find_Partition_By_Path("/vendor");
   if (Partition == NULL)
   goto skip;
  if (!PartitionManager.Is_Mounted_By_Path("/vendor")) {
  if (!PartitionManager.Mount_By_Path("/vendor", false))
  goto skip;
  }
  path = "/vendor/default.prop";
  if (!TWFunc::Path_Exists(path)) goto skip;
   }
cmp = path + ".redwolf";
if (TWFunc::Path_Exists(cmp)) unlink(cmp.c_str());
if (verity)
Set_New_Ramdisk_Property(path, "ro.config.dmverity=false");
Set_New_Ramdisk_Property(path, "persist.sys.recovery_update=false");
if (DataManager::GetIntValue(RW_ENABLE_DEBUGGING)) {
if (Set_New_Ramdisk_Property(path, "ro.debuggable=1")) properties++;
}
else if (DataManager::GetIntValue(RW_DISABLE_DEBUGGING)) {
if (Set_New_Ramdisk_Property(path, "ro.debuggable=0")) properties++;
}
if (DataManager::GetIntValue(RW_ENABLE_ADB_RO)) {
if (Set_New_Ramdisk_Property(path, "ro.adb.secure=1")) properties++;
}
else if (DataManager::GetIntValue(RW_DISABLE_ADB_RO)) {
if (Set_New_Ramdisk_Property(path, "ro.adb.secure=0")) properties++;
}
if (DataManager::GetIntValue(RW_ENABLE_SECURE_RO)) {
if (Set_New_Ramdisk_Property(path, "ro.secure=1")) properties++;
}
else if (DataManager::GetIntValue(RW_DISABLE_SECURE_RO)) {
if (Set_New_Ramdisk_Property(path, "ro.secure=0")) properties++;
}
if (DataManager::GetIntValue(RW_ENABLE_MOCK_LOCATION)) {
if (Set_New_Ramdisk_Property(path, "ro.allow.mock.location=1")) properties++;
}
else if (DataManager::GetIntValue(RW_DISABLE_MOCK_LOCATION)) {
if (Set_New_Ramdisk_Property(path, "ro.allow.mock.location=0")) properties++;
}
skip:
if (!vendor_state)
   PartitionManager.UnMount_By_Path("/vendor", false);
    if (verity)
 gui_msg("wolf_dumwolf_dm_verity=Successfully patched DM-Verity");
    else if (dm_verity == 1)
 gui_msg("wolf_dumwolf_dm_verity_off=DM-Verity is not enabled");
    if (encrypt)
 gui_msg("wolf_dumwolf_encryption=Successfully patched forced encryption");
    else if (encryption == 1)
 gui_msg("wolf_dumwolf_encryption_off=Forced Encryption is not enabled");
 gui_msg(Msg("wolf_dumwolf_properties=Modified '{1}' properties")(properties));
if (!Repack_Image("/boot")) {
gui_msg(Msg(msg::kProcess, "wolf_run_process_fail=Unable to finish '{1}' process")("Dumwolf"));
TWFunc::SetPerformanceMode(false);
return;
}
gui_msg(Msg(msg::kProcess, "wolf_run_process_done=Finished '{1}' process")("Dumwolf"));
TWFunc::SetPerformanceMode(false);
time(&stop);
int total_time = (int) difftime(stop, start);
LOGINFO("Dumwolf took %i second(s).\n", total_time);
return;
}