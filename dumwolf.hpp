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

#ifndef _RWRPDUMWOLF_HPP
#define _RWRPDUMWOLF_HPP

#include <string>
using str_t = std::string;

enum Ramdisk_Compression {
	UNKNOWN = 1,
	GZIP = 0,
	LZ4 = 2,
	BZIP2 = 4,
	XZ = 6,
	LZOP = 8,
	LZMA = 10
};

class RWDumwolf
{
public:
    static bool Repack_Image(const str_t mount_point);
    static bool Unpack_Image(const str_t mount_point);
	static void Deactivation_Process(void);
	static bool Resize_By_Path(const str_t& path);
	private:
	static bool Execute(const str_t& exec);
	static bool Patch_Encryption(const str_t& path, const str_t& fstab);
    static str_t Load_File(const str_t& extension);
    static bool Set_New_Ramdisk_Property(const str_t& default_prop, const str_t prop);
    static void Remove_Start_Of_Service(const str_t& file_path, const str_t service);
    static str_t Find_Ramdisk(void);
    static bool Is_Ramdisk_Unpacked(void);
    static Ramdisk_Compression Get_Ramdisk_Compression(const str_t& filename);
    static bool Check_Double_String(const str_t& filename, const str_t first, const str_t second, bool both);
	static void Handle_Boot_File_Type(const str_t& input, const str_t& local, str_t &output);
};

#endif
	