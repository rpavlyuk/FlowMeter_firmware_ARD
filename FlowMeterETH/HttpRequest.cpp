/*
  HttpRequest.cpp - library for handling HTTP Request on Arduino Web Server
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

// Parsing status
#define HTTP_PARSE_INIT		0		//Initial Parser Status
#define HTTP_METHOD 		0		//Parse the Method: GET POST UPDATE etc
#define HTTP_URI			1		//Parse the URI
#define HTTP_GET_NAME		11		//Parse the GET parameter NAME
#define HTTP_GET_VALUE		12		//Parse the GET parameter VALUE
#define HTTP_VERSION 		2		//Parse the version: HTTP1.1
#define HTTP_NEW_LINE		3		//Starts reading a new line
#define HTTP_ATTRIB_NAME	41		//Read the attibutes NAME
#define HTTP_ATTRIB_VALUE	42		//Read the attribute VALUE
#define HTTP_POST_NAME		51		//Read the POST parameter NAME
#define HTTP_POST_VALUE		52		//Read the POST paramenter VALUE
#define HTTP_COOKIE_NAME	61		//Read the COOKIE NAME
#define HTTP_COOKIE_VALUE	62		//Read the COOKIE VALUE
#define HTTP_REQUEST_END	99		//Finished reading the HTTP Request

#include "HttpRequest.h"

HttpRequest::HttpRequest() {
	resetRequest();
}

void HttpRequest::resetRequest() {

	freeParamMem(firstParam);
	freeCookieMem(firstCookie);

	parseStatus=HTTP_PARSE_INIT;
	method[0]='\0';
	uri[0]='\0';
	version[0]='\0';
	firstParam=NULL;
	firstCookie=NULL;
	paramCount=0;
	cookieCount=0;
	tmpParamName[0]='\0';
	tmpParamValue[0]='\0';
	tmpAttribName[0]='\0';
	tmpAttribValue[0]='\0';
	tmpCookieName[0]='\0';
	tmpCookieValue[0]='\0';
	dataBlockLength=0;
	dataCount=0;
}

void HttpRequest::freeParamMem(Params *paramNode){
	if(paramNode!=NULL){ 
		freeParamMem(paramNode->next);
		delete paramNode;
	}
}

void HttpRequest::freeCookieMem(Cookies *cookieNode){
	if(cookieNode!=NULL){ 
		freeCookieMem(cookieNode->next);
		delete cookieNode;
	}
}

void HttpRequest::parseRequest(char c){

	char cStr[2];
	cStr[0]=c;
	cStr[1]='\0';
	
	switch(parseStatus){
	
		case HTTP_METHOD:
			if(c==' ') parseStatus=HTTP_URI;
			else if(strlen(method)<HTTP_REQ_METHOD_LENGTH-1) strcat(method,cStr);
			break;
			
		case HTTP_URI:
			if(c==' ') parseStatus=HTTP_VERSION;
			else if (c=='?') parseStatus=HTTP_GET_NAME;
				 else if(strlen(uri)<HTTP_REQ_URI_LENGTH-1) strcat(uri,cStr);
			break;

		case HTTP_GET_NAME:
			if(c==' ') parseStatus=HTTP_VERSION;
			else if(c!='=') { if(strlen(tmpParamName)<HTTP_REQ_PARAM_NAME_LENGTH-1) strcat(tmpParamName,cStr); }
				 else parseStatus=HTTP_GET_VALUE;
			break;
			
		case HTTP_GET_VALUE:
			if(c=='&') { addParam(); parseStatus=HTTP_GET_NAME; }
			else if(c==' ') { addParam(); parseStatus=HTTP_VERSION; }
				 else if(strlen(tmpParamValue)<HTTP_REQ_PARAM_VALUE_LENGTH-1) strcat(tmpParamValue,cStr);
			break;
		
		case HTTP_VERSION:
			if(c=='\n') parseStatus=HTTP_NEW_LINE;
			else if(c!='\r' && strlen(version)<HTTP_REQ_VERSION_LENGTH-1) strcat(version,cStr);
			break;
					
		case HTTP_NEW_LINE:
			if(c!='\r' && c!='\n'){
				parseStatus=HTTP_ATTRIB_NAME;
				tmpAttribName[0]='\0';
				tmpAttribValue[0]='\0';
			}
			else { if(strcmp(method,"POST")==0 && dataBlockLength>0) parseStatus=HTTP_POST_NAME;
				   else parseStatus=HTTP_REQUEST_END;
				   break;
				 }
						
		case HTTP_ATTRIB_NAME:
			if(c=='\n') parseStatus=HTTP_NEW_LINE;
			else if(c!=':' && c!=' ' && c!='\r'){ if(strlen(tmpAttribName)<HTTP_REQ_ATTRIB_NAME_LENGTH-1) strcat(tmpAttribName,cStr); }
				 else if(c==' ') {
							if(strcmp(tmpAttribName,"Cookie")==0) parseStatus=HTTP_COOKIE_NAME;
							else parseStatus=HTTP_ATTRIB_VALUE;				 
					  }
			break;
						
		case HTTP_ATTRIB_VALUE:
			if(c=='\n') { addAttrib(); parseStatus=HTTP_NEW_LINE; }
			else if(c!='\r') if(strlen(tmpAttribValue)<HTTP_REQ_ATTRIB_VALUE_LENGTH-1) strcat(tmpAttribValue,cStr);
			break;
			
		case HTTP_POST_NAME:
			dataCount++;
			if(c!='=') { if(strlen(tmpParamName)<HTTP_REQ_PARAM_NAME_LENGTH-1) strcat(tmpParamName,cStr);}
			else parseStatus=HTTP_POST_VALUE;
			if (dataCount>dataBlockLength) { addParam(); parseStatus=HTTP_REQUEST_END; }
			break;
			
		case HTTP_POST_VALUE:
			dataCount++;
			if(c=='&') { addParam(); parseStatus=HTTP_POST_NAME; }
			else if(strlen(tmpParamValue)<HTTP_REQ_PARAM_VALUE_LENGTH-1) strcat(tmpParamValue,cStr);
			if(dataCount>dataBlockLength) { addParam(); parseStatus=HTTP_REQUEST_END; }
			break;
			
		case HTTP_COOKIE_NAME:
			if(c=='\n') parseStatus=HTTP_NEW_LINE;
			else if(c!='=') { if(c!=' ' && strlen(tmpCookieName)<HTTP_REQ_COOKIE_NAME_LENGTH-1) strcat(tmpCookieName,cStr); }
				 else parseStatus=HTTP_COOKIE_VALUE;
			break;
			
		case HTTP_COOKIE_VALUE:
			if(c==';') { addCookie(); parseStatus=HTTP_COOKIE_NAME; }
			else if(c=='\n') { addCookie(); parseStatus=HTTP_NEW_LINE; }
				 else if(c!='\r' && c!=' ' && strlen(tmpCookieValue)<HTTP_REQ_COOKIE_VALUE_LENGTH-1) strcat(tmpCookieValue,cStr);
			break;
	}
}

int HttpRequest::endOfRequest(){
	if(parseStatus==HTTP_REQUEST_END) return true;
	else return false;
}

void HttpRequest::addParam(){

	Params **cursor;

	cursor=&firstParam;
	while((*cursor)!=NULL) {
		if(strcmp((*cursor)->name,tmpParamName)==0)	break;
		cursor=&((*cursor)->next);
	}
	if((*cursor)==NULL) {
		(*cursor)=new Params;
		strcpy((*cursor)->name,tmpParamName);
		strcpy((*cursor)->value,tmpParamValue);
		(*cursor)->next=NULL;
		paramCount++;
	}
	
	tmpParamName[0]='\0';
	tmpParamValue[0]='\0';
}

void HttpRequest::addCookie(){

	Cookies **cursor;

	cursor=&firstCookie;
	while((*cursor)!=NULL) {
		if(strcmp((*cursor)->name,tmpCookieName)==0) break;
		cursor=&((*cursor)->next);
	}
	if((*cursor)==NULL) {
		(*cursor)=new Cookies;
		strcpy((*cursor)->name,tmpCookieName);
		strcpy((*cursor)->value,tmpCookieValue);
		(*cursor)->next=NULL;
		cookieCount++;
	}
	
	tmpCookieName[0]='\0';
	tmpCookieValue[0]='\0';
}

void HttpRequest::addAttrib(){
	
	if(strcmp(tmpAttribName,"Content-Length")==0)
		dataBlockLength=atoi(tmpAttribValue);
	
	tmpAttribName[0]='\0';
	tmpAttribValue[0]='\0';
}

int HttpRequest::getParam(int paramNum, char *name, char *value){
	int i=0;

	Params **cursor;
	
	cursor=&firstParam;
	while((*cursor)!=NULL) {
		i++;
		if(i==paramNum){
			strcpy(name,(*cursor)->name);
			strcpy(value,(*cursor)->value);	
			break;
		}
		cursor=&((*cursor)->next);
	}
	
	return i;
}

int HttpRequest::getParam(char *name, char *value){

	int pos=0, i=0;
	Params **cursor;

	cursor=&firstParam;
	while((*cursor)!=NULL) {
		i++;
		if(strcmp((*cursor)->name,name)==0) {
			strcpy(value,(*cursor)->value);
			pos=i;
			break;
		}
		cursor=&((*cursor)->next);
	}
	return pos;
}

int HttpRequest::getCookie(int cookieNum, char *name, char *value){
	int i=0;

	Cookies **cursor;
	
	cursor=&firstCookie;
	while((*cursor)!=NULL) {
		i++;
		if(i==cookieNum){
			strcpy(name,(*cursor)->name);
			strcpy(value,(*cursor)->value);
			break;
		}
		cursor=&((*cursor)->next);
	}
	return i;
}

int HttpRequest::getCookie(char *name, char *value){

	int pos=0,i=0;
	Cookies **cursor;

	cursor=&firstCookie;
	while((*cursor)!=NULL) {
		i++;
		if(strcmp((*cursor)->name,name)==0) {
			strcpy(value,(*cursor)->value);
			pos=i;
			break;
		}
		cursor=&((*cursor)->next);
	}
	return pos;
}

