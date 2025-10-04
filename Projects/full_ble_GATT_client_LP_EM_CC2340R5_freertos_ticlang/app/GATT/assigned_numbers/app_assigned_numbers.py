"""

@file  app_assigned_numbers.py

Group: WCS, BTS
Target Device: cc23xx

******************************************************************************

 Copyright (c) 2022-2025, Texas Instruments Incorporated
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions
 are met:

 *  Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

 *  Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

 *  Neither the name of Texas Instruments Incorporated nor the names of
    its contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

******************************************************************************

"""

""" This script converts the UUID from the bluetooth SIG assigned number yaml files into C dictionaries
The source files used are 
https://bitbucket.org/bluetooth-SIG/public/src/main/assigned_numbers/uuids/characteristic_uuids.yaml 
and
https://bitbucket.org/bluetooth-SIG/public/src/main/assigned_numbers/uuids/service_uuids.yaml
"""

# Usage : python app_assigned_number.py path_to_file_to_convert.yaml

import sys

class UUIDStruct:
	def __init__(self, uuid, name):
		self.uuid = uuid
		self.name = name

def main():
	file_path = sys.argv[1]

	with open(file_path, "r", encoding="utf8") as file:
		file_content = file.read()

		# Parse the file content to extract UUIDs and names
		uuid_list = []
		entries = file_content.split('uuids:\n')[1].strip().split('- uuid:')[1:]

		for entry in entries:
			uuid_match = entry.split('\n')[0].strip()
			name_match = entry.split('name:')[1].split('\n')[0].strip()
			uuid_list.append(UUIDStruct(uuid=uuid_match, name=name_match))

		# Generate the C-like dictionary representation
		c_dictionary = """
typedef struct 
{
	uint16_t uuid;
	const char *name;
} UUIDEntry;

static UUIDEntry UUIDdictionary[] = {
		"""

		for uuid_struct in uuid_list:
			c_dictionary += f"""    {{{uuid_struct.uuid}, "{uuid_struct.name}"}},
"""
		c_dictionary += "};"

		print(c_dictionary)
		


if __name__ == "__main__":
	if len(sys.argv) != 2:
		print("Usage : python app_assigned_number.py path_to_file_to_convert.yaml")
		sys.exit(-1)

	main()