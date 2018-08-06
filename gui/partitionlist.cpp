/*
	Copyright 2013 bigbiff/Dees_Troy TeamWin
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

extern "C" {
#include "../twcommon.h"
}
#include "../minuitwrp/minui.h"

#include "rapidxml.hpp"
#include "objects.hpp"
#include "../data.hpp"
#include "../partitions.hpp"

GUIPartitionList::GUIPartitionList(xml_node<>* node) : GUIScrollList(node)
{
	xml_attribute<>* attr;
	xml_node<>* child;

	mIconSelected = mIconUnselected = NULL;
	mUpdate = 0;
	updateList = false;

	child = FindNode(node, "icon");
	if (child)
	{
		mIconSelected = LoadAttrImage(child, "selected");
		mIconUnselected = LoadAttrImage(child, "unselected");
	}

	// Handle the result variable
	child = FindNode(node, "data");
	if (child)
	{
		attr = child->first_attribute("name");
		if (attr)
			mVariable = attr->value();
		attr = child->first_attribute("selectedlist");
		if (attr)
			selectedList = attr->value();
	}

	int iconWidth = std::max(mIconSelected->GetWidth(), mIconUnselected->GetWidth());
	int iconHeight = std::max(mIconSelected->GetHeight(), mIconUnselected->GetHeight());
	SetMaxIconSize(iconWidth, iconHeight);

	child = FindNode(node, "listtype");
	if (child && (attr = child->first_attribute("name"))) {
		ListType = attr->value();
		updateList = true;
	} else {
		mPartList.clear();
		mAppList.clear();
		LOGERR("No partition listtype specified for partitionlist GUI element\n");
		return;
	}
}

GUIPartitionList::~GUIPartitionList()
{
}

int GUIPartitionList::Update(void)
{
	if (!isConditionTrue())
		return 0;

	// Check for changes in mount points if the list type is mount and update the list and render if needed
	if (ListType == "mount") {
		int listSize = mPartList.size();
		for (int i = 0; i < listSize; i++) {
			if (PartitionManager.Is_Mounted_By_Path(mPartList.at(i).Mount_Point) && !mPartList.at(i).selected) {
				mPartList.at(i).selected = 1;
				mUpdate = 1;
			} else if (!PartitionManager.Is_Mounted_By_Path(mPartList.at(i).Mount_Point) && mPartList.at(i).selected) {
				mPartList.at(i).selected = 0;
				mUpdate = 1;
			}
		}
	}

	GUIScrollList::Update();

	if (ListType == "restore_app") {
		bool refreshlist = DataManager::GetIntValue("rw_refresh_restore");
		if (refreshlist) {			
			updateList = true;
		}
	}
	
	if (updateList) {
		// Completely update the list if needed -- Used primarily for
		// restore as the list for restore will change depending on what
		// partitions were backed up
		mPartList.clear();
		mAppList.clear();
		if (ListType == "backup_app" || ListType == "restore_app") {
			PartitionManager.Get_App_List(ListType, &mAppList);
			DataManager::SetValue("rw_refresh_restore","0");}
		else				
			PartitionManager.Get_Partition_List(ListType, &mPartList);
		SetVisibleListLocation(0);
		updateList = false;
		mUpdate = 1;
		if (ListType == "backup" || ListType == "flashimg" || ListType == "ose")
			MatchList();
	}

	if (mUpdate) {
		mUpdate = 0;
		if (Render() == 0)
			return 2;
	}

	return 0;
}

int GUIPartitionList::NotifyVarChange(const std::string& varName, const std::string& value)
{
	GUIScrollList::NotifyVarChange(varName, value);

	if (!isConditionTrue())
		return 0;

	if (varName == mVariable && !mUpdate)
	{
		if (ListType == "osestorage" || ListType == "storage") {
			currentValue = value;
			SetPosition();
		} else if (ListType == "backup" || ListType == "ose") {
			MatchList();
		} else if (ListType == "restore") {
			updateList = true;
			SetVisibleListLocation(0);
		}

		mUpdate = 1;
		return 0;
	}
	return 0;
}

void GUIPartitionList::SetPageFocus(int inFocus)
{
	if (ListType != "backup_app" && ListType != "restore_app") {
		GUIScrollList::SetPageFocus(inFocus);
		if (inFocus) {
			if (ListType == "osestorage" || ListType == "storage" || ListType == "flashimg") {
				DataManager::GetValue(mVariable, currentValue);
				SetPosition();
			}
		}
	}
}

void GUIPartitionList::MatchList(void) {
	int i, listSize = mPartList.size();
	string variablelist, searchvalue;
	size_t pos;

	DataManager::GetValue(mVariable, variablelist);

	for (i = 0; i < listSize; i++) {
		searchvalue = mPartList.at(i).Mount_Point + ";";
		pos = variablelist.find(searchvalue);
		if (pos != string::npos) {
			mPartList.at(i).selected = 1;
		} else {
			mPartList.at(i).selected = 0;
		}
	}
}

void GUIPartitionList::SetPosition() {
	int listSize = mPartList.size();

	SetVisibleListLocation(0);
	for (int i = 0; i < listSize; i++) {
		if (mPartList.at(i).Mount_Point == currentValue) {
			mPartList.at(i).selected = 1;
			SetVisibleListLocation(i);
		} else {
			mPartList.at(i).selected = 0;
		}
	}
}

size_t GUIPartitionList::GetItemCount()
{
	return (ListType == "backup_app" || ListType == "restore_app") ? mAppList.size() : mPartList.size();
}

void GUIPartitionList::RenderItem(size_t itemindex, int yPos, bool selected)
{
	// note: the "selected" parameter above is for the currently touched item
	// don't confuse it with the more persistent "selected" flag per list item used below
	ImageResource* icon;
	if (ListType == "backup_app" || ListType == "restore_app")
		icon = mAppList.at(itemindex).selected ? mIconSelected : mIconUnselected;
	else
		icon = mPartList.at(itemindex).selected ? mIconSelected : mIconUnselected;
	std::string txt;
	if (ListType == "backup_app" || ListType == "restore_app")
		txt = (mAppList.at(itemindex).App_Name == "") ? mAppList.at(itemindex).Pkg_Name : mAppList.at(itemindex).App_Name;
	else
		txt = mPartList.at(itemindex).Display_Name;
	const std::string& text = txt;
	RenderStdItem(yPos, selected, icon, text.c_str());
}

void GUIPartitionList::NotifySelect(size_t item_selected)
{
	bool isAppList = (ListType == "backup_app" || ListType == "restore_app");
	int listSize = isAppList ? mAppList.size() : mPartList.size();
	
	if (item_selected < listSize) {		
		if (ListType == "mount") {
			if (!mPartList.at(item_selected).selected) {
				if (PartitionManager.Mount_By_Path(mPartList.at(item_selected).Mount_Point, true)) {
					mPartList.at(item_selected).selected = 1;
					PartitionManager.Add_MTP_Storage(mPartList.at(item_selected).Mount_Point);
					mUpdate = 1;
				}
			} else {
				if (PartitionManager.UnMount_By_Path(mPartList.at(item_selected).Mount_Point, true)) {
					mPartList.at(item_selected).selected = 0;
					mUpdate = 1;
				}
			}
		} else if (!mVariable.empty()) {
			if (ListType == "storage" || ListType == "osestorage") {
				int i;
				std::string str = mPartList.at(item_selected).Mount_Point;
				bool update_size = false;
				
				TWPartition* Part = PartitionManager.Find_Partition_By_Path(str);
				if (Part == NULL) {
					LOGERR("Unable to locate partition for '%s'\n", str.c_str());
					return;
				}
				if (!Part->Is_Mounted() && Part->Removable)
					update_size = true;
				if (!Part->Mount(true)) {
					// Do Nothing
				} else if (update_size && !Part->Update_Size(true)) {
					// Do Nothing
				} else {
					for (i=0; i<listSize; i++)
						mPartList.at(i).selected = 0;
					
					if (update_size) {
						char free_space[255];
						sprintf(free_space, "%llu", Part->Free / 1024 / 1024);
						mPartList.at(item_selected).Display_Name = Part->Storage_Name + " (";
						mPartList.at(item_selected).Display_Name += free_space;
						mPartList.at(item_selected).Display_Name += "MB)";
					}
					mPartList.at(item_selected).selected = 1;
					mUpdate = 1;
					
					DataManager::SetValue(mVariable, str);
				}
			} else if (isAppList) {				
				if (mAppList.at(item_selected).selected)
					mAppList.at(item_selected).selected = 0;
				else
					mAppList.at(item_selected).selected = 1;
				
				int i;
				string variablelist;
				for (i=0; i<listSize; i++) {
					if (mAppList.at(i).selected) {
						variablelist += mAppList.at(i).Pkg_Name + ";";
					}
				}
				
				mUpdate = 1;
				if (selectedList.empty())
					DataManager::SetValue(mVariable, variablelist);
				else
					DataManager::SetValue(selectedList, variablelist);				
			} else {
				if (ListType == "flashimg") { // only one item can be selected for flashing images
					for (int i=0; i<listSize; i++)
						mPartList.at(i).selected = 0;
				}
				if (mPartList.at(item_selected).selected)
					mPartList.at(item_selected).selected = 0;
				else
					mPartList.at(item_selected).selected = 1;
				
				int i;
				string variablelist;
				for (i=0; i<listSize; i++) {
					if (mPartList.at(i).selected) {
						variablelist += mPartList.at(i).Mount_Point + ";";
					}
				}
				
				mUpdate = 1;
				if (selectedList.empty())
					DataManager::SetValue(mVariable, variablelist);
				else
					DataManager::SetValue(selectedList, variablelist);
			}
		}
	}
}
