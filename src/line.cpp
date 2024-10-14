/********************************************************************************
Project:      The Hydraulic Economic River System Simulator (HERSS)
Filename:     line.cpp
Developer:    Bernt Viggo Matheussen (Bernt.Viggo.Matheussen@aenergi.no)
Organization: Å Energi, www.ae.no

This software is released under the MIT license:

Copyright (c) <2024> <Å Energi, Bernt Viggo Matheussen>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
********************************************************************************/

#include "herss.h"

Line::Line() {}
Line::~Line() {}

////////////////////////////////////////////////////////////////////
string Line::extractNextElementFromLine(string* line)
{
	int tmp = line->find_first_not_of(DELIMITER); 		// Erase whitespace in front,
	line->erase(0, tmp);					// if any.
	tmp = line->find_first_of(DELIMITER);
	string result =  line->substr(0,tmp);			// Extract element
	line->find_first_not_of(DELIMITER, tmp);
	line->erase(0, tmp);					// Erase element and whitespace
	return result;
}
////////////////////////////////////////////////////////////////////
// Calculates number of coloumns in line
int Line::calcNrCols(string* line)
{
	int cols;
	int end_of_line;
	string tmp_str;
	string word;
	tmp_str = *line;
	end_of_line =  0;
	cols=0;
	while (end_of_line ==0) {
        word = extractNextElementFromLine(&tmp_str);
        if( word.length() > 0 ) cols++;
        if( tmp_str.length() < 1 ) return cols;
        if(cols > MAX_WORDS ) exit(0);
    }
    return cols;
}
////////////////////////////////////////////////////////////////////
// Method extracts the last word from the line
// it also removes it from the input line
string Line::extractLastElementFromLine(string* line)
{
    int tmp;
    string result;

	tmp = line->find_first_not_of(DELIMITER); 		// Erase white space in front
	line->erase(0, tmp);

	tmp = line->find_last_not_of(DELIMITER); 		// Erase white space at the end
	line->erase(tmp+1, line->length() );
	tmp = line->find_last_of(DELIMITER);
	
    if(int(tmp) < 0) { 
        // No delimiter found, we return the string itself.
        result = line->substr(0,line->length());
        line->erase(0, line->length());
        return result;
    }

    result =  line->substr(tmp,line->length());     // Extract last element
	line->erase(tmp, line->length());			    // Erase element and whitespace
  	tmp = result.find_first_not_of(DELIMITER); 		// Erase whitespace in front
	result.erase(0, tmp);
	return result;
}
////////////////////////////////////////////////////////////////////
// Method remove white space at beginning and end of string
int Line::removeWhites(string* line)
{
    int tmp;
	tmp = line->find_first_not_of(DELIMITER); 		// Erase whitespace in front, if any
	line->erase(0, tmp);
	tmp = line->find_last_not_of(DELIMITER); 		// Erase whitespace at the end, if any
    line->erase(tmp+1, line->length() );
	return 0;
}
////////////////////////////////////////////////////////////////////
// Checks if string contains a digit.
int Line::checkDigit(string line){
    int myint;
    for(int c=0; c< int(line.length()); c++) {
        myint = isdigit(line[c]);
        if(myint != 0 ) return 1;
    }
	return 0;
}
////////////////////////////////////////////////////////////////////
