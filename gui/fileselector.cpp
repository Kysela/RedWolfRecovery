/*
    Copyright 2018 ATG Droid/Dadi11 RedWolf
	This file is part of RWRP/RedWolf Recovery Project.
	
	Copyright 2012 bigbiff/Dees_Troy TeamWin
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

#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <algorithm>
#include <vector>

extern "C" {
#include "../twcommon.h"
}
#include "../minuitwrp/minui.h"

#include "rapidxml.hpp"
#include "objects.hpp"
#include "../data.hpp"
#include "../twrp-functions.hpp"
#include "../adbbu/libtwadbbu.hpp"

int GUIFileSelector::mSortOrder = 0;

using str_t = std::string;

GUIFileSelector::GUIFileSelector(xml_node<>* node) : GUIScrollList(node)
{
	xml_attribute<>* attr;
	xml_node<>* child;

	mFileSelectionIcon = mFileUnSelectionIcon = mDirSelectionIcon = mDirUnSelectionIcon = mZipSelectionIcon = mZipUnSelectionIcon = NULL;
	mShowFolders = mShowFiles = mShowNavFolders = 1;
	mUpdate = 0;
	mPathVar = "cwd";
	updateFileList = false;

	// Load filter for filtering files (e.g. *.zip for only zips)
	child = FindNode(node, "filter");
	if (child) {
		attr = child->first_attribute("extn");
		if (attr)
			mExtn = attr->value();
		attr = child->first_attribute("folders");
		if (attr)
			mShowFolders = atoi(attr->value());
		attr = child->first_attribute("files");
		if (attr)
			mShowFiles = atoi(attr->value());
		attr = child->first_attribute("nav");
		if (attr)
			mShowNavFolders = atoi(attr->value());
	}

	// Handle the path variable
	child = FindNode(node, "path");
	if (child) {
		attr = child->first_attribute("name");
		if (attr)
			mPathVar = attr->value();
		attr = child->first_attribute("default");
		if (attr) {
			mPathDefault = attr->value();
			DataManager::SetValue(mPathVar, attr->value());
		}
	}

	// Handle the result variable
	child = FindNode(node, "data");
	if (child) {
		attr = child->first_attribute("name");
		if (attr)
			mVariable = attr->value();
		attr = child->first_attribute("default");
		if (attr)
			DataManager::SetValue(mVariable, attr->value());
	}

	// Handle the sort variable
	child = FindNode(node, "sort");
	if (child) {
		attr = child->first_attribute("name");
		if (attr)
			mSortVariable = attr->value();
		attr = child->first_attribute("default");
		if (attr)
			DataManager::SetValue(mSortVariable, attr->value());

		DataManager::GetValue(mSortVariable, mSortOrder);
	}

	child = FindNode(node, "selection");
	if (child) {
        attr = child->first_attribute("name");
       if (attr)
		mSelection = attr->value();
	else
		mSelection = "0";
		attr = child->first_attribute("multiple");
	    if (attr)
		is_FileManager = true;
	else
		is_FileManager = false;
	  }

	// Get folder and file icons if present
	child = FindNode(node, "icon");
	if (child) {
		mFileSelectionIcon = LoadAttrImage(child, "fileselected");
		mFileUnSelectionIcon = LoadAttrImage(child, "fileunselected");
		mDirSelectionIcon = LoadAttrImage(child, "dirselected");
		mDirUnSelectionIcon = LoadAttrImage(child, "dirunselected");
		mZipSelectionIcon = LoadAttrImage(child, "zipselected");
		mZipUnSelectionIcon = LoadAttrImage(child, "zipunselected");
	}
	int iconWidth = std::max(mDirSelectionIcon->GetWidth(), mFileSelectionIcon->GetWidth());
	int iconHeight = std::max(mDirSelectionIcon->GetHeight(), mFileSelectionIcon->GetHeight());
	SetMaxIconSize(iconWidth, iconHeight);

	// Fetch the file/folder list
	GetFileList(DataManager::GetStrValue(mPathVar));
}

GUIFileSelector::~GUIFileSelector()
{
}

int GUIFileSelector::ObjectInFileList(const std::string& object, bool files)
{
	for (const auto& mFileList : (files ? mMultipleFileList : mMultipleDirList)) {
      if (mFileList == object)
      return 0;
      }
    return 1;
}

int GUIFileSelector::Update(void)
{
	if (!isConditionTrue())
		return 0;

	GUIScrollList::Update();

	// Update the file list if needed
	if (updateFileList) {
		if (mSelectionList == 1) {
			updateFileList = false;
	        mUpdate = 1;
	      } else {
		if (GetFileList(DataManager::GetStrValue(mPathVar)) == 0) {
			updateFileList = false;
			mUpdate = 1;
		} else
			return 0;
	}
  }
  if (is_FileManager) {
  int mFileManagerRender = 0;
  DataManager::GetValue("rw_filemanager_cleanup_render", mFileManagerRender);
  if (mFileManagerRender == 1) {
  mMultipleFileList.clear();
  mMultipleDirList.clear();
  DataManager::SetValue("rw_filemanager_cleanup_render", "0");
  if (GetFileList(DataManager::GetStrValue(mPathVar)) != 0) return 0;
  if (Render() == 0)
   return 2;
   else
   return 0;
  }
 }
 
	if (mUpdate) {
		mUpdate = 0;
		if (Render() == 0)
			return 2;
	}
	return 0;
}

int GUIFileSelector::NotifyVarChange(const std::string& varName, const std::string& value)
{
	GUIScrollList::NotifyVarChange(varName, value);

	if (!isConditionTrue())
		return 0;

	if (varName.empty()) {
		// Always clear the data variable so we know to use it
		DataManager::SetValue(mVariable, "");
	}
	if (varName == mPathVar || varName == mSortVariable) {
		if (varName == mSortVariable) {
			DataManager::GetValue(mSortVariable, mSortOrder);
		} else {
			// Reset the list to the top
			SetVisibleListLocation(0);
			if (value.empty())
				DataManager::SetValue(mPathVar, mPathDefault);
		}
		updateFileList = true;
		mUpdate = 1;
		return 0;
	}
	return 0;
}

bool GUIFileSelector::fileSort(FileData d1, FileData d2)
{
	if (d1.fileName == "..")
		return -1;
	if (d2.fileName == "..")
		return 0;

	switch (mSortOrder) {
		case 3: // by size largest first
			if (d1.fileSize == d2.fileSize || d1.fileType == DT_DIR) // some directories report a different size than others - but this is not the size of the files inside the directory, so we just sort by name on directories
				return (strcasecmp(d1.fileName.c_str(), d2.fileName.c_str()) < 0);
			return d1.fileSize < d2.fileSize;
		case -3: // by size smallest first
			if (d1.fileSize == d2.fileSize || d1.fileType == DT_DIR) // some directories report a different size than others - but this is not the size of the files inside the directory, so we just sort by name on directories
				return (strcasecmp(d1.fileName.c_str(), d2.fileName.c_str()) > 0);
			return d1.fileSize > d2.fileSize;
		case 2: // by last modified date newest first
			if (d1.lastModified == d2.lastModified)
				return (strcasecmp(d1.fileName.c_str(), d2.fileName.c_str()) < 0);
			return d1.lastModified < d2.lastModified;
		case -2: // by date oldest first
			if (d1.lastModified == d2.lastModified)
				return (strcasecmp(d1.fileName.c_str(), d2.fileName.c_str()) > 0);
			return d1.lastModified > d2.lastModified;
		case -1: // by name descending
			return (strcasecmp(d1.fileName.c_str(), d2.fileName.c_str()) > 0);
		default: // should be a 1 - sort by name ascending
			return (strcasecmp(d1.fileName.c_str(), d2.fileName.c_str()) < 0);
	}
	return 0;
}

int GUIFileSelector::GetFileList(const std::string folder)
{
	DIR* d;
	struct dirent* de;
	struct stat st;
    
	// Clear all data
	mFolderList.clear();
	mFileList.clear();
    
	d = opendir(folder.c_str());
	if (d == NULL) {
		LOGINFO("Unable to open '%s'\n", folder.c_str());
		if (folder != "/" && (mShowNavFolders != 0 || mShowFiles != 0)) {
			size_t found;
			found = folder.find_last_of('/');
			if (found != str_t::npos) {
				str_t new_folder = folder.substr(0, found);

				if (new_folder.length() < 2)
					new_folder = "/";
				DataManager::SetValue(mPathVar, new_folder);
			}
		}
		return -1;
	}

	while ((de = readdir(d)) != NULL) {
		
		if (!strcmp(de->d_name, "."))
        continue;
        if (!strcmp(de->d_name, "..") && folder == "/")
        continue;
      
		FileData data;
		
		data.fileName = de->d_name;
		data.fileType = de->d_type;

		str_t path = folder + "/" + data.fileName;
		stat(path.c_str(), &st);
		data.protection = st.st_mode;
		data.userId = st.st_uid;
		data.groupId = st.st_gid;
		data.fileSize = st.st_size;
		data.lastAccess = st.st_atime;
		data.lastModified = st.st_mtime;
		data.lastStatChange = st.st_ctime;

		if (data.fileType == DT_UNKNOWN) {
			data.fileType = TWFunc::Get_D_Type_From_Stat(path);
		}
		if (data.fileType == DT_DIR) {
			if (mShowNavFolders || strcmp(de->d_name, ".."))
				mFolderList.push_back(data);
		} else if (data.fileType == DT_REG || data.fileType == DT_LNK || data.fileType == DT_BLK) {
			if (mExtn.empty() || (data.fileName.length() > mExtn.length() && data.fileName.substr(data.fileName.length() - mExtn.length()) == mExtn)) {
				if (mExtn == ".ab" && twadbbu::Check_ADB_Backup_File(path))
					mFolderList.push_back(data);
				else
					mFileList.push_back(data);
			}
		}
	}
	closedir(d);

	std::sort(mFolderList.begin(), mFolderList.end(), fileSort);
	std::sort(mFileList.begin(), mFileList.end(), fileSort);

	return 0;
}

void GUIFileSelector::SetPageFocus(int inFocus)
{
	GUIScrollList::SetPageFocus(inFocus);
	if (inFocus) {
		if (DataManager::GetStrValue(mPathVar).empty())
			DataManager::SetValue(mPathVar, mPathDefault);
		updateFileList = true;
		mUpdate = 1;
	}
}

size_t GUIFileSelector::GetItemCount()
{
	return (mShowFolders ? mFolderList.size() : 0) + (mShowFiles ? mFileList.size() : 0);
}

void GUIFileSelector::RenderItem(size_t itemindex, int yPos, bool selected)
{
	size_t folderSize = mShowFolders ? mFolderList.size() : 0;

	ImageResource* icon;
	str_t text;

	if (itemindex < folderSize) {
		text = mFolderList.at(itemindex).fileName;
		if (is_FileManager) {
        if (!ObjectInFileList(text, false))
		icon = mDirSelectionIcon;
		else
		icon = mDirUnSelectionIcon;
		}
		else
		icon = mDirUnSelectionIcon;
		if (text == "..")
			text = gui_lookup("up_a_level", "(Up A Level)");
	} else {
		text = mFileList.at(itemindex - folderSize).fileName;
		size_t extension = text.find_last_of(".");
		if (is_FileManager) {
        if (!ObjectInFileList(text, true)) {
        if (extension != str_t::npos && text.substr(extension) == ".zip")
		icon = mZipSelectionIcon;
		else
		icon = mFileSelectionIcon;
		} else {
		if (extension != str_t::npos && text.substr(extension) == ".zip")
		icon = mZipUnSelectionIcon;
		else
		icon = mFileUnSelectionIcon;
		}
		}
		else if (extension != str_t::npos && text.substr(extension) == ".zip")
		icon = mZipUnSelectionIcon;
		else
		icon = mFileUnSelectionIcon;
	}

	RenderStdItem(yPos, selected, icon, text.c_str());
}

void GUIFileSelector::NotifySelect(size_t item_selected)
{
	size_t folderSize = mShowFolders ? mFolderList.size() : 0;
	size_t fileSize = mShowFiles ? mFileList.size() : 0;
	if (is_FileManager) {
	bool cleanup = mSelectionList == 1;
	DataManager::GetValue("rw_filemanager_allow_selection", mSelectionList);
	if (cleanup && !mSelectionList) {
	mMultipleFileList.clear();
    mMultipleDirList.clear();
    }
   }
	if (item_selected < folderSize + fileSize) {
		// We've selected an item!
		str_t str;
		if (item_selected < folderSize) {
			str_t cwd;
            DataManager::GetValue(mPathVar, cwd);
            
			str = mFolderList.at(item_selected).fileName;
			if (mSelection != "0") {
				if (mSelectionList == 1 && is_FileManager) {
					if (str == "..") {
				mMultipleFileList.clear();
                mMultipleDirList.clear();
				DataManager::SetValue("rw_filemanager_allow_selection", "0");
				DataManager::SetValue("rw_filemanager_hide_button", "0");
				mSelectionList = 0;
				goto request;
				}
		       if (ObjectInFileList(str, false))
	               mMultipleDirList.push_back(str);
	               else
		           mMultipleDirList.erase(std::remove(mMultipleDirList.begin(),mMultipleDirList.end(), str), mMultipleDirList.end());           
		         if (mMultipleDirList.empty() && mMultipleFileList.empty()) {
		           DataManager::SetValue("rw_filemanager_hide_button", "1"); 
		           return;
		           } else {
			       DataManager::SetValue("rw_filemanager_hide_button", "0");
                   } 
				if (cwd != "/") cwd += "/";
		        str_t files;
                for (const auto& mFileList : mMultipleDirList)
                files += "\"" + cwd + mFileList + "\" ";
                DataManager::SetValue("rw_multiple_dir_list", files); 
                return;
				   } else {
                     DataManager::SetValue(mSelection, str);
                 }
                }
                request:
                // Ignore requests to do nothing
			if (str == ".") return;
			// if (!mSelectionList) {
			if (str == "..") {
				if (cwd != "/") {
					size_t found;
					found = cwd.find_last_of('/');
					cwd = cwd.substr(0,found);

					if (cwd.length() < 2)   cwd = "/";
				}
			} else {
				// Add a slash if we're not the root folder
				if (cwd != "/") cwd += "/";
				cwd += str;
			}

			if (mShowNavFolders == 0 && (mShowFiles == 0 || mExtn == ".ab")) {
				// this is probably the restore list and we need to save chosen location to mVariable instead of mPathVar
				DataManager::SetValue(mVariable, cwd);
			} else {
				// We are changing paths, so we need to set mPathVar
				DataManager::SetValue(mPathVar, cwd);
			}
		// }
		} else if (!mVariable.empty()) {
			str = mFileList.at(item_selected - folderSize).fileName;
			str_t cwd;
			DataManager::GetValue(mPathVar, cwd);
			if (cwd != "/")
			cwd += "/";
			if (mSelection != "0") {
				if (mSelectionList == 1 && is_FileManager) {
		        if (ObjectInFileList(str, true))
	            mMultipleFileList.push_back(str);
	            else
                mMultipleFileList.erase(std::remove(mMultipleFileList.begin(),mMultipleFileList.end(), str), mMultipleFileList.end());
                if (mMultipleDirList.empty() && mMultipleFileList.empty()) {
		           DataManager::SetValue("rw_filemanager_hide_button", "1"); 
		           return;
		           } else {
			       DataManager::SetValue("rw_filemanager_hide_button", "0");
                   } 
	            str_t files;
                for (const auto& mFileList : mMultipleFileList)
                files += "\"" + cwd + mFileList + "\" ";
                DataManager::SetValue("rw_multiple_file_list", files);
                return;
				} else {
				if (is_FileManager) {
				mMultipleFileList.clear();
                mMultipleFileList.push_back(str);
				DataManager::SetValue("rw_multiple_list_type", str.substr(str.size() - 4) == ".zip" ? "3" : "0");
				DataManager::SetValue("rw_multiple_file_list", cwd + str);
				}
                DataManager::SetValue(mSelection, str);
                }
               }
			DataManager::SetValue(mVariable, cwd + str);
		}
	}
	mUpdate = 1;
}			