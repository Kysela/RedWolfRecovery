/*
	Copyright 2017 to 2018 ATG Droid/Dadi11 RedWolf
	This file is part of RWRP/RedWolf Recovery Project.
	
	Copyright 2012 to 2017 bigbiff/Dees_Troy TeamWin
	This file is part of TWRP/TeamWin Recovery Project.
	
	TWRP is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.
	TWRP is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	You should have received a copy of the GNU General Public License
	along with TWRP.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <iostream>
#include <fstream>

#include <string.h>
#include <stdio.h>

#include "twcommon.h"
#include "mtdutils/mounts.h"
#include "mtdutils/mtdutils.h"

#ifdef USE_MINZIP
#include "minzip/SysUtil.h"
#else
#include "otautil/SysUtil.h"
#include <ziparchive/zip_archive.h>
#endif
#include "zipwrap.hpp"
#ifdef USE_OLD_VERIFIER
#include "verifier24/verifier.h"
#else
#include "verifier.h"
#endif
#include "variables.h"
#include "cutils/properties.h"
#include "data.hpp"
#include "partitions.hpp"
#include "twrpDigestDriver.hpp"
#include "twrpDigest/twrpDigest.hpp"
#include "twrpDigest/twrpMD5.hpp"
#include "twrp-functions.hpp"
#include "gui/gui.hpp"
#include "gui/pages.hpp"
#include "gui/blanktimer.hpp"
#include "legacy_property_service.h"
#include "twinstall.h"
#include "set_metadata.h"
#include "installcommand.h"
extern "C" {
	#include "gui/gui.h"
}

#define AB_OTA "payload_properties.txt"

static const char* properties_path = "/dev/__properties__";
static const char* properties_path_renamed = "/dev/__properties_kk__";
static bool legacy_props_env_initd = false;
static bool legacy_props_path_modified = false;

enum zip_type {
	UNKNOWN_ZIP_TYPE = 0,
	UPDATE_BINARY_ZIP_TYPE,
	AB_OTA_ZIP_TYPE
};

static std::string get_survival_path()
{
	std::string path = DataManager::GetOseStoragePath();
	if (!PartitionManager.Mount_By_Path(path, false))
	path = DataManager::GetSettingsStoragePath();
    return path + "/WOLF/.BACKUPS/OSE";
}

static bool storage_is_encrypted(const std::string& path)
{
return (DataManager::GetSettingsStoragePath() == TWFunc::Get_Root_Path(path) && DataManager::GetIntValue(TW_IS_ENCRYPTED) != 0);
}

static bool ors_is_active()
{
return (DataManager::GetStrValue("tw_action") == "openrecoveryscript");
}

// to support pre-KitKat update-binaries that expect properties in the legacy format
static int switch_to_legacy_properties()
{
	if (!legacy_props_env_initd) {
		if (legacy_properties_init() != 0)
			return -1;

		char tmp[32];
		int propfd, propsz;
		legacy_get_property_workspace(&propfd, &propsz);
		sprintf(tmp, "%d,%d", dup(propfd), propsz);
		setenv("ANDROID_PROPERTY_WORKSPACE", tmp, 1);
		legacy_props_env_initd = true;
	}

	if (TWFunc::Path_Exists(properties_path)) {
		// hide real properties so that the updater uses the envvar to find the legacy format properties
		if (rename(properties_path, properties_path_renamed) != 0) {
			LOGERR("Renaming %s failed: %s\n", properties_path, strerror(errno));
			return -1;
		} else {
			legacy_props_path_modified = true;
		}
	}

	return 0;
}

static int switch_to_new_properties()
{
	if (TWFunc::Path_Exists(properties_path_renamed)) {
		if (rename(properties_path_renamed, properties_path) != 0) {
			LOGERR("Renaming %s failed: %s\n", properties_path_renamed, strerror(errno));
			return -1;
		} else {
			legacy_props_path_modified = false;
		}
	}

	return 0;
}

static std::string load_ota_fingerprint(const std::string& path)
{
      std::string line;
      ifstream File;
      File.open (path);
      if (File.is_open()) {
       getline(File,line);
       File.close();
       }
       return line;
}

static void set_install_status(const int st, bool& zip_is_rom_package)
{
if (!ors_is_active() || !zip_is_rom_package)
return;
std::string install_status;
switch (st) {
  case INSTALL_SUCCESS:
  install_status = "INSTALL_SUCCESS";
  break;
  case INSTALL_ERROR:
  install_status = "INSTALL_ERROR";
  break; 
  case INSTALL_CORRUPT:
  install_status = "INSTALL_CORRUPT";
  break;
  case INSTALL_VERIFY_FAILURE:
  install_status = "INSTALL_VERIFY_FAILURE";
  break;
  case INSTALL_RETRY:
  install_status = "INSTALL_RETRY";
  break;
  default:
  LOGERR("Unknown install status %i\n", st);
  return;
  }
     ofstream status;
     status.open (TMP_INSTALL_STATUS);
     status << install_status << '\n';
	 status.close();    
	 chmod(TMP_INSTALL_STATUS, 0755);
}

bool ExtractToBuffer(ZipWrap* Zip, const char* entry, std::string* buffer) {
    const long size = Zip->GetUncompressedSize(entry);
    if (size <= 0)
		return false;

    buffer->resize(size, '\0');
    if (!Zip->ExtractToBuffer(entry, reinterpret_cast<uint8_t*>(&(*buffer)[0]))) {
    	LOGINFO("Failed to extract entry '%s' to buffer\n", entry);
        return false;
    }
    return true;
}


static std::string get_metadata_property(std::vector<std::string>& metadata, const std::string Property) {
    size_t i, end_pos;
	for (i=0;i<metadata.size();i++) {
	if (metadata.at(i)[0] != Property[0]) continue;
		end_pos = metadata.at(i).find("=", 0);
		if (metadata.at(i).substr(0, end_pos) == Property)
		return metadata.at(i).substr(end_pos + 1, string::npos);
	   }
	return "";
}
	

static bool verify_incremental_package(const string& fingerprint, const string& metadatafp, const string& metadatadevice)
{
	return (((metadatafp != fingerprint) ||
	(!metadatadevice.empty() && fingerprint.find(metadatadevice) == string::npos) ||
	(!metadatadevice.empty() && !metadatafp.empty() && metadatafp.find(metadatadevice) == string::npos))
	? false : true);
}

/*
Implementation of OTA Survival process: Copyright 2017 to 2018 RedWolf Recovery Project


             RedWolf: 025: Magua: Improve some parts
             RedWolf  026: Magua: Push the new parts of the detection and backup/restore system
             RedWolf: 027: BigDaddyDave: Update previous base and improve it
             RedWolf: 028: ATG Droid: Completely rebuild everything, add suport for A/B, allow OSE backup to be stored on multiple storages and create new detection
             
             
             TO-DO: Handle already defined calls for verification property
                         :startx::call_undefined_variable_calls // unsigned long long KLTM
                         :hwservicemanager::handle_buffer_overflow
                         :::REDWOLF::START_ * 0X72 * L_STAT - STARTX
             
*/

static int Prepare_OTA_Survival(const char *path, ZipWrap *Zip, zip_type ztype, bool &zip_is_rom_package, std::string &metadata_fingerprint) 
{
         if (DataManager::GetIntValue(RW_INCREMENTAL_PACKAGE) != 0) {
          bool zip_is_survival_trigger = false;
          const std::string fingerprint_property = "ro.build.fingerprint";
          std::string metadata_device, meta_data, fingerprint;
          std::vector<std::string> metadata;
          gui_msg("wolf_install_detecting=Detecting Current Package");
	     if (Zip->EntryExists(METADATA) && ExtractToBuffer(Zip, METADATA, &meta_data)) {
		  metadata = TWFunc::Split_String(meta_data, "\n", true);
	      metadata_fingerprint = get_metadata_property(metadata, "pre-build");
		  metadata_device = get_metadata_property(metadata, "pre-device");
		if (!metadata_fingerprint.empty()) {
          fingerprint = TWFunc::System_Property_Get(fingerprint_property);
          zip_is_survival_trigger = true;
          }
          }
	      if (ztype == UPDATE_BINARY_ZIP_TYPE) {
		   if (DataManager::GetIntValue("wolf_ose_force_zip_entry") == 0) {
		   LOGINFO("OSE: Using call of block_image_update for the detection\n");
		   std::string updater_buffer;              
		   std::vector<std::string> updater;
		  if (Zip->EntryExists(UPDATER_SCRIPT) && ExtractToBuffer(Zip, UPDATER_SCRIPT, &updater_buffer)) {
	       updater = TWFunc::Split_String(updater_buffer, "\n", true);
	       for (size_t i = 0;i < updater.size();i++) {
		   if (updater.at(i).find("block_image_update") != std::string::npos) {
			zip_is_rom_package = true;
			break;
			}
		}
	}
	} else {
		string entry;
		DataManager::GetValue(RW_MAIN_SURVIVAL_TRIGGER, entry);
		LOGINFO("OSE Restore: Using defined ZIP entry for the detection: '%s'\n", entry.c_str());
		if (Zip->EntryExists(entry))
		zip_is_rom_package = true;
		}
         } else if (ztype == AB_OTA_ZIP_TYPE) {
       if (!metadata_fingerprint.empty() || !get_metadata_property(metadata, "post-build").empty())
         	zip_is_rom_package = true;           
    }   
       if (zip_is_rom_package)
       gui_msg("wolf_install_miui_detected=- Detected Survival Trigger Package");
       else
       gui_msg("wolf_install_standard_detected=- Detected standard Package");        
       
	   gui_msg("wolf_incremental_ota_status_enabled=Support MIUI Incremental package status: Enabled");
	
	    if (zip_is_survival_trigger) {
		gui_msg(Msg("wolf_incremental_package_detected=Detected Incremental package '{1}'")(path));
		
		if (!fingerprint.empty()) {
        if (DataManager::GetIntValue("wolf_verify_incremental_ota_signature") != 0) {
		gui_msg("wolf_incremental_ota_compatibility_chk=Verifying Incremental Package Signature...");
		if (verify_incremental_package(fingerprint, metadata_fingerprint, metadata_device)) {
		gui_msg("wolf_incremental_ota_compatibility_true=Incremental package is compatible.");
		property_set(fingerprint_property.c_str(), metadata_fingerprint.c_str());
	    } else {
		gui_err("wolf_incremental_ota_compatibility_false=Incremental package isn't compatible with this ROM!");
		return INSTALL_VERIFY_FAILURE;
		}
		} else {
		property_set(fingerprint_property.c_str(), metadata_fingerprint.c_str());
		}
		}
        const std::string survival_folder = get_survival_path(), path = survival_folder + "/redwolf.info";
        LOGINFO("OSE Restore: Defined OSE folder path: '%s'\n", survival_folder.c_str());
	  if (storage_is_encrypted(survival_folder)) {
		gui_err("wolf_survival_encrypted_err=Internal storage is encrypted! Please do decrypt first!");
        return INSTALL_ERROR;
        }
        if (!TWFunc::Path_Exists(survival_folder + "/.")) {
        	gui_err("wolf_survival_does_not_exist=Unable to find OSE Restore folder!");
            return INSTALL_ERROR;
            }
            PartitionManager.Set_Restore_Files(survival_folder);
            if (DataManager::GetStrValue("tw_restore_list").empty()) {
            LOGERR("OSE Restore: Unable to find any partitions in the OSE folder!\n");
            return INSTALL_ERROR;
            }
       if (TWFunc::Path_Exists(path) && load_ota_fingerprint(path) == metadata_fingerprint) {
          LOGINFO("OSE Restore: Signature matched\n");
          zip_is_rom_package = false;
          return INSTALL_SUCCESS;
         } 
        gui_msg(Msg(msg::kProcess, "wolf_run_process=Starting '{1}' process")("OSE Restore"));
		if (!PartitionManager.Run_OTA_Survival_Restore(survival_folder)) {
        gui_msg(Msg(msg::kProcess, "wolf_run_process_fail=Unable to finish '{1}' process")("OSE Restore"));
        return INSTALL_ERROR;
        } else {
        gui_msg(Msg(msg::kProcess, "wolf_run_process_done=Finished '{1}' process")("OSE Restore"));
        zip_is_rom_package = zip_is_survival_trigger;
        }
   }
  } else {
	gui_msg("wolf_incremental_ota_status_disabled=Support MIUI Incremental package status: Disabled");
  }
  return INSTALL_SUCCESS;
  }

static int Prepare_Update_Binary(const char *path, ZipWrap *Zip, int* wipe_cache) {

	if (!Zip->ExtractEntry(ASSUMED_UPDATE_BINARY_NAME, TMP_UPDATER_BINARY_PATH, 0755)) {
		Zip->Close();
		LOGERR("Could not extract '%s'\n", ASSUMED_UPDATE_BINARY_NAME);
		return INSTALL_ERROR;
	}		
	
	 if (blankTimer.isScreenOff()) {
   if (Zip->EntryExists(AROMA_CONFIG)) {
		blankTimer.toggleBlank();
		gui_changeOverlay("");
		}
      }
         
		// If exists, extract file_contexts from the zip file
	if (!Zip->EntryExists("file_contexts")) {
		Zip->Close();
		LOGINFO("Zip does not contain SELinux file_contexts file in its root.\n");
	} else {
		const string output_filename = "/file_contexts";
		LOGINFO("Zip contains SELinux file_contexts file in its root. Extracting to %s\n", output_filename.c_str());
		if (!Zip->ExtractEntry("file_contexts", output_filename, 0644)) {
			Zip->Close();
			LOGERR("Could not extract '%s'\n", output_filename.c_str());
			return INSTALL_ERROR;
		}
	}
	Zip->Close();
	return INSTALL_SUCCESS;
}

static bool update_binary_has_legacy_properties(const char *binary) {
	const char str_to_match[] = "ANDROID_PROPERTY_WORKSPACE";
	bool found = false;

	int fd = open(binary, O_RDONLY);
	if (fd < 0) {
		LOGINFO("has_legacy_properties: Could not open %s: %s!\n", binary, strerror(errno));
		return false;
	}

	struct stat finfo;
	if (fstat(fd, &finfo) < 0) {
		LOGINFO("has_legacy_properties: Could not fstat %d: %s!\n", fd, strerror(errno));
		close(fd);
		return false;
	}

	void *data = mmap(NULL, finfo.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (data == MAP_FAILED) {
		LOGINFO("has_legacy_properties: mmap (size=%lld) failed: %s!\n", finfo.st_size, strerror(errno));
	} else {
		if (memmem(data, finfo.st_size, str_to_match, sizeof(str_to_match) - 1)) {
			LOGINFO("has_legacy_properties: Found legacy property match!\n");
			found = true;
		}
		munmap(data, finfo.st_size);
	}
	close(fd);

	return found;
}

static int Run_Update_Binary(const char *path, ZipWrap *Zip, int* wipe_cache, zip_type ztype) {
	int ret_val, pipe_fd[2], status, zip_verify;
	char buffer[1024];
	FILE* child_data;

#ifndef TW_NO_LEGACY_PROPS
	if (!update_binary_has_legacy_properties(TMP_UPDATER_BINARY_PATH)) {
		LOGINFO("Legacy property environment not used in updater.\n");
	} else if (switch_to_legacy_properties() != 0) { /* Set legacy properties */
		LOGERR("Legacy property environment did not initialize successfully. Properties may not be detected.\n");
	} else {
		LOGINFO("Legacy property environment initialized.\n");
	}
#endif

	pipe(pipe_fd);

	std::vector<std::string> args;
    if (ztype == UPDATE_BINARY_ZIP_TYPE) {
		ret_val = update_binary_command(path, 0, pipe_fd[1], &args);
    } else if (ztype == AB_OTA_ZIP_TYPE) {
		ret_val = abupdate_binary_command(path, Zip, 0, pipe_fd[1], &args);
	} else {
		LOGERR("Unknown zip type %i\n", ztype);
		ret_val = INSTALL_CORRUPT;
	}
    if (ret_val) {
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return ret_val;
    }

	// Convert the vector to a NULL-terminated char* array suitable for execv.
	const char* chr_args[args.size() + 1];
	chr_args[args.size()] = NULL;
	for (size_t i = 0; i < args.size(); i++)
		chr_args[i] = args[i].c_str();

	pid_t pid = fork();
	if (pid == 0) {
		close(pipe_fd[0]);
		execve(chr_args[0], const_cast<char**>(chr_args), environ);
		printf("E:Can't execute '%s': %s\n", chr_args[0], strerror(errno));
		_exit(-1);
	}
	close(pipe_fd[1]);

	*wipe_cache = 0;

	DataManager::GetValue(TW_SIGNED_ZIP_VERIFY_VAR, zip_verify);
	child_data = fdopen(pipe_fd[0], "r");
	while (fgets(buffer, sizeof(buffer), child_data) != NULL) {
		char* command = strtok(buffer, " \n");
		if (command == NULL) {
			continue;
		} else if (strcmp(command, "progress") == 0) {
			char* fraction_char = strtok(NULL, " \n");
			char* seconds_char = strtok(NULL, " \n");

			float fraction_float = strtof(fraction_char, NULL);
			int seconds_float = strtol(seconds_char, NULL, 10);

			if (zip_verify)
				DataManager::ShowProgress(fraction_float * (1 - VERIFICATION_PROGRESS_FRAC), seconds_float);
			else
				DataManager::ShowProgress(fraction_float, seconds_float);
		} else if (strcmp(command, "set_progress") == 0) {
			char* fraction_char = strtok(NULL, " \n");
			float fraction_float = strtof(fraction_char, NULL);
			DataManager::SetProgress(fraction_float);
		} else if (strcmp(command, "ui_print") == 0) {
			char* display_value = strtok(NULL, "\n");
			if (display_value) {
				gui_print("%s", display_value);
       } else {
				gui_print("\n");
			}
		} else if (strcmp(command, "wipe_cache") == 0) {
			*wipe_cache = 1;
		} else if (strcmp(command, "log") == 0) {
			printf("%s\n", strtok(NULL, "\n"));
		} else if (strcmp(command, "clear_display") == 0) {
		// Do nothing, not supported by TWRP
		} else {
			LOGERR("unknown command [%s]\n", command);
		}
	}
	fclose(child_data);

	int waitrc = TWFunc::Wait_For_Child(pid, &status, "Updater");

#ifndef TW_NO_LEGACY_PROPS
	/* Unset legacy properties */
	if (legacy_props_path_modified) {
		if (switch_to_new_properties() != 0) {
			LOGERR("Legacy property environment did not disable successfully. Legacy properties may still be in use.\n");
		} else {
			LOGINFO("Legacy property environment disabled.\n");
		}
	}
#endif
	return (waitrc == 0 ? INSTALL_SUCCESS : INSTALL_ERROR);
}

int TWinstall_zip(const char* path, int* wipe_cache) {
	int ret_val, zip_verify = 1;
	bool zip_is_rom_package = false;
    std::string metadata_fingerprint;

	if (strcmp(path, "error") == 0) {
		LOGERR("Failed to get adb sideload file: '%s'\n", path);
		return INSTALL_CORRUPT;
	}
	
	gui_msg(Msg("installing_zip=Installing zip file '{1}'")(path));
	if (strlen(path) < 9 || strncmp(path, "/sideload", 9) != 0) {
		string digest_str;
		string Full_Filename = path;
		string digest_file = path;
		string defmd5file = digest_file + ".md5sum";

		if (TWFunc::Path_Exists(defmd5file)) {
			digest_file += ".md5sum";
		}
		else {
			digest_file += ".md5";
		}

		gui_msg("check_for_digest=Checking for Digest file...");
		if (!TWFunc::Path_Exists(digest_file)) {
			gui_msg("no_digest=Skipping Digest check: no Digest file found");
		}
		else {
			if (TWFunc::read_file(digest_file, digest_str) != 0) {
				LOGERR("Skipping MD5 check: MD5 file unreadable\n");
			}
			else {
				twrpDigest *digest = new twrpMD5();
				if (!twrpDigestDriver::stream_file_to_digest(Full_Filename, digest)) {
					delete digest;
					set_install_status(INSTALL_CORRUPT, zip_is_rom_package);
					return INSTALL_CORRUPT;
				}
				string digest_check = digest->return_digest_string();
				if (digest_str == digest_check) {
					gui_msg(Msg("digest_matched=Digest matched for '{1}'.")(path));
				}
				else {
					LOGERR("Aborting zip install: Digest verification failed\n");
					set_install_status(INSTALL_CORRUPT, zip_is_rom_package);
					delete digest;
					return INSTALL_CORRUPT;
				}
				delete digest;
			}
		}
	}

#ifndef TW_OEM_BUILD
	DataManager::GetValue(TW_SIGNED_ZIP_VERIFY_VAR, zip_verify);
#endif
    if (get_workspace())
    return check_property_workspace();
	DataManager::SetProgress(0);
	MemMapping map;
#ifdef USE_MINZIP
	if (sysMapFile(path, &map) != 0) {
#else
	if (!map.MapFile(path)) {
#endif
		gui_msg(Msg(msg::kError, "fail_sysmap=Failed to map file '{1}'")(path));
		return INSTALL_VERIFY_FAILURE;
	}

	if (zip_verify) {
		gui_msg("verify_zip_sig=Verifying zip signature...");
#ifdef USE_OLD_VERIFIER
		ret_val = verify_file(map.addr, map.length);
#else
		std::vector<Certificate> loadedKeys;
		if (!load_keys("/res/keys", loadedKeys)) {
			LOGINFO("Failed to load keys");
			gui_err("verify_zip_fail=Zip signature verification failed!");
			set_install_status(INSTALL_VERIFY_FAILURE, zip_is_rom_package);
#ifdef USE_MINZIP
			sysReleaseMap(&map);
#endif
			return INSTALL_VERIFY_FAILURE;
		}
		ret_val = verify_file(map.addr, map.length, loadedKeys, std::bind(&DataManager::SetProgress, std::placeholders::_1));
#endif
		if (ret_val != VERIFY_SUCCESS) {
			LOGINFO("Zip signature verification failed: %i\n", ret_val);
			gui_err("verify_zip_fail=Zip signature verification failed!");
			set_install_status(INSTALL_VERIFY_FAILURE, zip_is_rom_package);
#ifdef USE_MINZIP
			sysReleaseMap(&map);
#endif
			return INSTALL_VERIFY_FAILURE;
		} else {
			gui_msg("verify_zip_done=Zip signature verified successfully.");
		}
	
}

	
	ZipWrap Zip;
	if (!Zip.Open(path, &map)) {
		set_install_status(INSTALL_CORRUPT, zip_is_rom_package);
		gui_err("zip_corrupt=Zip file is corrupt!");
#ifdef USE_MINZIP
			sysReleaseMap(&map);
#endif
		return INSTALL_CORRUPT;
	}
	
	time_t start, stop;
	time(&start);
	if (Zip.EntryExists(ASSUMED_UPDATE_BINARY_NAME)) {
		LOGINFO("Update binary zip\n");
		// Additionally verify the compatibility of the package.
		if (!verify_package_compatibility(&Zip)) {
			gui_err("zip_compatible_err=Zip Treble compatibility error!");
			Zip.Close();
#ifdef USE_MINZIP
			sysReleaseMap(&map);
#endif
			ret_val = INSTALL_CORRUPT;
		} else {
			while (true) {
			ret_val = Prepare_OTA_Survival(path, &Zip, UPDATE_BINARY_ZIP_TYPE, zip_is_rom_package, metadata_fingerprint);
			if (ret_val != INSTALL_SUCCESS)
			break;
			
			ret_val = Prepare_Update_Binary(path, &Zip, wipe_cache);
			if (ret_val != INSTALL_SUCCESS)
			break;
			
			ret_val = Run_Update_Binary(path, &Zip, wipe_cache, UPDATE_BINARY_ZIP_TYPE);
			break;
	  }
	}
	} else {
		if (Zip.EntryExists(AB_OTA)) {
			LOGINFO("AB zip\n"); 
			ret_val = Prepare_OTA_Survival(path, &Zip, AB_OTA_ZIP_TYPE, zip_is_rom_package, metadata_fingerprint);
			if (ret_val == INSTALL_SUCCESS)
			ret_val = Run_Update_Binary(path, &Zip, wipe_cache, AB_OTA_ZIP_TYPE);
		} else {
				Zip.Close();
				ret_val = INSTALL_CORRUPT;
		}
	}
	if (ret_val == INSTALL_CORRUPT) {
		gui_err("invalid_zip_format=Invalid zip file format!");
	       } else {
		if (ret_val == INSTALL_SUCCESS && zip_is_rom_package && DataManager::GetIntValue(RW_INCREMENTAL_PACKAGE) != 0) {
		 gui_msg(Msg(msg::kProcess, "wolf_run_process=Starting '{1}' process")("OSE Backup"));
	     const std::string survival_folder = get_survival_path(), path = survival_folder + "/redwolf.info";
	     LOGINFO("OSE Backup: Defined OSE folder path: '%s'\n", survival_folder.c_str());
	     if (storage_is_encrypted(survival_folder)) {
        gui_err("wolf_survival_encrypted_err=Internal storage is encrypted! Please do decrypt first!");
        goto fail;
        }
         if (TWFunc::Path_Exists(path) && load_ota_fingerprint(path) == metadata_fingerprint) {
         gui_msg("wolf_ose_same_singature=Detected already existing OSE Backup of the current package!");
         goto skip;
            }
		if (TWFunc::Path_Exists(survival_folder + "/.")) 
		TWFunc::removeDir(survival_folder, true);
		else if (!TWFunc::Recursive_Mkdir(survival_folder)) goto fail;
		if (!PartitionManager.Run_OTA_Survival_Backup(survival_folder))
        goto fail;
		if (!metadata_fingerprint.empty()) {
		std::ofstream fp;
		fp.open (path.c_str());
	    fp << metadata_fingerprint << '\n';
	    fp.close();
        tw_set_default_metadata(path.c_str());
        }
		goto skip;
	fail:
	gui_msg(Msg(msg::kProcess, "wolf_run_process_fail=Unable to finish '{1}' process")("OSE Backup"));
    ret_val = INSTALL_ERROR;
    goto finish;
	skip:
	gui_msg(Msg(msg::kProcess, "wolf_run_process_done=Finished '{1}' process")("OSE Backup"));
	}
	
	      finish:
	      time(&stop);
	      int total_time = (int) difftime(stop, start);
	      set_install_status(ret_val, zip_is_rom_package);
	      LOGINFO("Install took %i second(s).\n", total_time);
	}
#ifdef USE_MINZIP
	sysReleaseMap(&map);
#endif
	return ret_val;
}
