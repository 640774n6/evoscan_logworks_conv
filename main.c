//
// What:	evoscan_logworks_conv - main.c
// Why:		Converts Evoscan CSV to Logworks DIF
// Who:		Brandon Holland (bholland@brandon-holland.com, @640774n6)
// Where:	http://brandon-holland.com
// When:	May 11, 2014
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>

#include "libcsv.h"

const double sample_time = 0.082;
const char *ignore_fields[5] = { "LogID", "LogEntryDate", "LogEntryTime", "LogEntrySeconds", "LogNotes" };
const int colors[18] =
{
	13369344,	//red
	204,		//blue
	52224,		//green
	16776960,	//yellow
	16750848,	//orange
	10053120,	//brown
	10027161,	//purple
	16751052,	//pink
	6684672,	//dark red
	102,		//dark blue
	26112,		//dark green
	8421376,	//dark yellow
	11683584,	//dark orange
	4993792,	//dark brown
	4980812,	//dark purple
	15108700,	//dark peach
	9737364,	//grey
	0,			//black
};

int csv_col, csv_row;
int row_sample_count;
int total_sample_count;
double total_sample_time;

char **fields;
int field_count;
char **used_fields;
int used_field_count;
int *used_fields_min;
int *used_fields_max;

#pragma mark -
#pragma mark Functions
#pragma mark

int should_ignore_field(char *field)
{
	for(int i = 0; i < 5; i++)
	{
		if(!strcmp(field, ignore_fields[i]))
		{ return 1; }
	}
	return 0;
}

int color_for_index(int i)
{
	int c = i % 18;
	return colors[c];
}

#pragma mark -
#pragma mark libcsv Callbacks
#pragma mark

void csv_process_col(void *value, size_t length, void *tmp_file)
{
	if(value)
	{
		//read all fields
		if(csv_row == 0)
		{
			size_t new_size = sizeof(char *) * (field_count + 1);
			fields = (char **)realloc(fields, new_size);
			fields[field_count] = NULL;
			
			if(!should_ignore_field(value))
			{
				size_t field_length = strlen(value) + 1;
				fields[field_count] = (char *)malloc(sizeof(char) * field_length);
				fields[field_count][(field_length - 1)] = '\0';
				strcpy(fields[field_count], value);
			}
			field_count++;
		}
		else
		{
			char *field = fields[csv_col];
			if(field)
			{
				//determine used fields
				if(csv_row == 1)
				{
					size_t new_size = sizeof(char *) * (used_field_count + 1);
					used_fields = (char **)realloc(used_fields, new_size);
				
					new_size = sizeof(int) * (used_field_count + 1);
					used_fields_min = (int *)realloc(used_fields_min, new_size);
					used_fields_max = (int *)realloc(used_fields_max, new_size);
					
					used_fields_min[used_field_count] = INT_MAX;
					used_fields_max[used_field_count] = INT_MIN;
					used_fields[used_field_count] = field;
					used_field_count++;
				}
				
				//write sample header
				if(row_sample_count == 0)
				{
					fprintf(tmp_file, "-1,0\r\nBOT\r\n0,%f\r\nV\r\n", total_sample_time);
					total_sample_time += sample_time;
					total_sample_count++;
				}
				
				size_t sample_length = strlen(value) + 1;
				char *sample = (char *)malloc(sizeof(char) * sample_length);
				sample[(sample_length - 1)] = '\0';
				strcpy(sample, value);
				
				//write field sample
				fprintf(tmp_file, "0,%s\r\nV\r\n", sample);
				
				//update min/max
				float value_float = atof(sample);
				int value_int = (int)value_float;
				
				if(value_int < used_fields_min[row_sample_count])
				{ used_fields_min[row_sample_count] = value_int; }
				
				if(value_int > used_fields_max[row_sample_count])
				{ used_fields_max[row_sample_count] = value_int; }
				
				row_sample_count++;
				free(sample);
			}
		}
	}
	csv_col++;
}

void csv_process_row(int c, void *tmp_file)
{
	//reset + update variables
	csv_col = 0;
	row_sample_count = 0;
	csv_row++;
}

#pragma mark -
#pragma mark Main Functon (Where it all starts)
#pragma mark

int main(int argc, char *argv[])
{
	//check arguments
	if(argc != 3)
	{
		printf("usage: evolog <input csv path> <output dif path>\n");
		return 1;
	}

	char *input_path = argv[1];
	char *output_path = argv[2];
	if(!strcmp(input_path, output_path))
	{
		printf("error: input and output path must be different\n");
		return 1;
	}
	
	//open file pointers
	FILE *input_file = fopen(input_path, "rb");
	if(!input_file)
	{
		printf("error: failed to open input @ %s\n", input_path);
		return 1;
	}
	
	FILE *output_file = fopen(output_path, "wb");
	if(!output_path)
	{
		printf("error: failed to open output @ %s\n", output_path);
		fclose(input_file);
		return 1;
	}
	
	FILE *tmp_file = tmpfile();
	if(!tmp_file)
	{
		printf("error: failed to open tmp file\n");
		fclose(input_file);
		fclose(output_file);
		return 1;
	}
	
	//initialize variables
	fields = NULL;
	field_count = 0;
	used_fields = NULL;
	used_fields_min = NULL;
	used_fields_max = NULL;
	used_field_count = 0;
	total_sample_time = 0.0;
	total_sample_count = 0;
	row_sample_count = 0;
	csv_col = 0;
	csv_row = 0;
	
	//create csv parser
	struct csv_parser parser;
	unsigned char options = (CSV_APPEND_NULL | CSV_EMPTY_IS_NULL);
	csv_init(&parser, options);
	
	//main parse loop
	size_t length = 0;
	char buffer[1024];
	while((length = fread(buffer, 1, 1024, input_file)) > 0)
	{
		//parse csv and handle with callbacks
		if(csv_parse(&parser, buffer, length, csv_process_col, csv_process_row, tmp_file) != length)
		{
			printf("error: failed to read from input @ %s\n", input_path);
			fclose(input_file);
			fclose(output_file);
			fclose(tmp_file);
			csv_free(&parser);
			remove(output_path);
			return 1;
		}
	}
	
	//write output header
	fprintf(output_file, "TABLE\r\n0,1\r\n\"EXCEL\"\r\n");
	fprintf(output_file, "VECTORS\r\n0,%d\r\n\"\"\r\n", (total_sample_count + 13));
	fprintf(output_file, "TUPLES\r\n0,%d\r\n\"\"\r\n", (used_field_count + 1));
	fprintf(output_file, "DATA\r\n0,0\r\n\"\"\r\n");
	fprintf(output_file, "-1,0\r\nBOT\r\n");
	
	fprintf(output_file, "1,0\r\n\"Input Description\"\r\n");
	for(int i = 0; i < used_field_count; i++)
	{ fprintf(output_file, "1,0\r\n\"\"\r\n"); }
	fprintf(output_file, "-1,0\r\nBOT\r\n");
	
	fprintf(output_file, "1,0\r\n\"Stochiometric:\"\r\n");
	for(int i = 0; i < used_field_count; i++)
	{ fprintf(output_file, "1,0\r\n\"\"\r\n"); }
	fprintf(output_file, "-1,0\r\nBOT\r\n");
	
	fprintf(output_file, "1,0\r\n\"From Device:\"\r\n");
	for(int i = 0; i < used_field_count; i++)
	{ fprintf(output_file, "1,0\r\n\"(EVOSCAN%d)\"\r\n", (i + 1)); }
	fprintf(output_file, "-1,0\r\nBOT\r\n");
	
	fprintf(output_file, "1,0\r\n\"Name:\"\r\n");
	for(int i = 0; i < used_field_count; i++)
	{ fprintf(output_file, "1,0\r\n\"%s\"\r\n", used_fields[i]); }
	fprintf(output_file, "-1,0\r\nBOT\r\n");
	
	fprintf(output_file, "1,0\r\n\"Unit:\"\r\n");
	for(int i = 0; i < used_field_count; i++)
	{ fprintf(output_file, "1,0\r\n\"%.3s\"\r\n", used_fields[i]); }
	fprintf(output_file, "-1,0\r\nBOT\r\n");
	
	fprintf(output_file, "1,0\r\n\"Range:\"\r\n");
	for(int i = 0; i < used_field_count; i++)
	{ fprintf(output_file, "0,%d\r\nV\r\n", used_fields_min[i]); }
	fprintf(output_file, "-1,0\r\nBOT\r\n");
	
	fprintf(output_file, "1,0\r\n\"equiv(Sample):\"\r\n");
	for(int i = 0; i < used_field_count; i++)
	{ fprintf(output_file, "0,0\r\nV\r\n"); }
	fprintf(output_file, "-1,0\r\nBOT\r\n");
	
	fprintf(output_file, "1,0\r\n\"to:\"\r\n");
	for(int i = 0; i < used_field_count; i++)
	{ fprintf(output_file, "0,%d\r\nV\r\n", used_fields_max[i] + 1); }
	fprintf(output_file, "-1,0\r\nBOT\r\n");
	
	fprintf(output_file, "1,0\r\n\"equiv(Sample):\"\r\n");
	for(int i = 0; i < used_field_count; i++)
	{ fprintf(output_file, "0,4096\r\nV\r\n"); }
	fprintf(output_file, "-1,0\r\nBOT\r\n");
	
	fprintf(output_file, "1,0\r\n\"Color:\"\r\n");
	for(int i = 0; i < used_field_count; i++)
	{ fprintf(output_file, "0,%d\r\nV\r\n", color_for_index(i)); }
	fprintf(output_file, "-1,0\r\nBOT\r\n");
	
	fprintf(output_file, "1,0\r\n\"-End-\"\r\n");
	for(int i = 0; i < used_field_count; i++)
	{ fprintf(output_file, "1,0\r\n\"\"\r\n"); }
	fprintf(output_file, "-1,0\r\nBOT\r\n");
	
	fprintf(output_file, "1,0\r\n\"Session 1\"\r\n");
	for(int i = 0; i < used_field_count; i++)
	{ fprintf(output_file, "1,0\r\n\"\"\r\n"); }
	fprintf(output_file, "-1,0\r\nBOT\r\n");
	
	fprintf(output_file, "1,0\r\n\"Time(sec)\"\r\n");
	for(int i = 0; i < used_field_count; i++)
	{ fprintf(output_file, "1,0\r\n\"%s (%.3s)\"\r\n", used_fields[i], used_fields[i]); }
	
	//append tmp to the output
	fseek(tmp_file, 0, SEEK_SET);
	while((length = fread(buffer, 1, 1024, tmp_file)) > 0)
	{ fwrite(buffer, sizeof(char), length, output_file); }
	
	//write footer
	fprintf(output_file, "-1,0\r\nEOD\r\n");
	
	//free fields
	for(int i = 0; i < field_count; i++)
	{ free(fields[i]); }
	free(fields);
	free(used_fields);
	free(used_fields_min);
	free(used_fields_max);
	
	//close file pointers
	fclose(input_file);
	fclose(output_file);
	fclose(tmp_file);
	
	//free parser
	csv_free(&parser);
	
	return 0;
}
