/*
  HttpRequest.h - library for handling HTTP Request on Arduino Web Server
  Copyright (c) 2012 Francisco G. Paletta.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef HttpRequest_h
#define HttpRequest_h

#include <inttypes.h>
#include "Stream.h"

#define HTTP_REQ_METHOD_LENGTH 10		//10 is enough
#define HTTP_REQ_URI_LENGTH 32			//adjust if you have long path/file names
#define HTTP_REQ_VERSION_LENGTH 10		//10 is enough
#define HTTP_REQ_PARAM_NAME_LENGTH 64		//adjust to meet your needs
#define HTTP_REQ_PARAM_VALUE_LENGTH 64	//adjust to meet your needs
#define HTTP_REQ_ATTRIB_NAME_LENGTH 16	//enough to track attribute name
#define HTTP_REQ_ATTRIB_VALUE_LENGTH 16	//enough to track "Content-Length" value
#define HTTP_REQ_COOKIE_NAME_LENGTH 10	//adjust to meet your needs
#define HTTP_REQ_COOKIE_VALUE_LENGTH 16	//adjust to meet your needs


class HttpRequest
{
  public:
	struct Params{
		char name[HTTP_REQ_PARAM_NAME_LENGTH];
		char value[HTTP_REQ_PARAM_VALUE_LENGTH];
		Params *next;
	};
	
	struct Cookies{
		char name[HTTP_REQ_COOKIE_NAME_LENGTH];
		char value[HTTP_REQ_COOKIE_VALUE_LENGTH];
		Cookies *next;
	};
	
	int parseStatus;
	Params *firstParam;
	Cookies *firstCookie;
	
	char tmpParamName[HTTP_REQ_PARAM_NAME_LENGTH];
	char tmpParamValue[HTTP_REQ_PARAM_VALUE_LENGTH];
	char tmpAttribName[HTTP_REQ_ATTRIB_NAME_LENGTH];
	char tmpAttribValue[HTTP_REQ_ATTRIB_NAME_LENGTH];
	char tmpCookieName[HTTP_REQ_COOKIE_NAME_LENGTH];
	char tmpCookieValue[HTTP_REQ_COOKIE_VALUE_LENGTH];
	
	int dataBlockLength, dataCount;
	
	void addParam();
	void addAttrib();
	void addCookie();
	void freeParamMem(Params *paramNode);
	void freeCookieMem(Cookies *cookieNode);
	
  public:
	char method[HTTP_REQ_METHOD_LENGTH];
	char uri[HTTP_REQ_URI_LENGTH];
	char version[HTTP_REQ_VERSION_LENGTH];
	
	int paramCount;
	int cookieCount;
	
	HttpRequest();
	void resetRequest();
	void parseRequest(char c);
	int endOfRequest();
	int getParam(int paramNum, char *name, char *value);
	int getParam(char *name, char *value);
	int getCookie(int cookieNum, char *name, char *value);
	int getCookie(char *name, char *value);

};

#endif

