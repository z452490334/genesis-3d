/****************************************************************************
Copyright (c) 2006, Radon Labs GmbH
Copyright (c) 2011-2013,WebJet Business Division,CYOU
 
http://www.genesis-3d.com.cn

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
****************************************************************************/
#include "stdneb.h"
#include "exception/exceptions.h"
#include "io/xmlreader.h"

namespace IO
{
__ImplementClass(IO::XmlReader, 'XMLR', IO::StreamReader);

// This static object setsup TinyXml at application startup
// (set condense white space to false). There seems to be no easy,
// alternative way (see TinyXml docs for details)
XmlReader::TinyXmlInitHelper XmlReader::initTinyXml;

using namespace Util;

using namespace Math;

    
//------------------------------------------------------------------------------
/**
*/
XmlReader::XmlReader() :
    xmlDocument(0),
    curNode(0)
	
{
    // empty
}

//------------------------------------------------------------------------------
/**
*/
XmlReader::~XmlReader()
{
    if (this->IsOpen())
    {
        this->Close();
    }
}

//------------------------------------------------------------------------------
/**
    Opens the stream and reads the content of the stream into
    TinyXML.
*/
bool
XmlReader::Open()
{
    n_assert(0 == this->xmlDocument);
    n_assert(0 == this->curNode);

    if (StreamReader::Open())
    {
        // create an XML document object
        this->xmlDocument = n_new(TiXmlDocument);
        if (!this->xmlDocument->LoadStream(this->stream))
        {
            // parsing XML structure failed, provide feedback
            const URI& uri = this->stream->GetURI();
            if (uri.IsValid())
            {
                n_warning("XmlReader::Open(): failed to open stream as XML '%s'\nTinyXML error: %s!", uri.AsString().AsCharPtr(), this->xmlDocument->ErrorDesc());
            }
            else
            {
                n_warning("XmlReader::Open(): failed to open stream as XML (URI not valid)!\nTinyXML error: %s", this->xmlDocument->ErrorDesc());
            }
			return false;
			// something happen in the LoadStream function, memory has freed
			n_delete(this->xmlDocument);
			this->xmlDocument = 0;
        }

        // since the XML document is now loaded, we can close the original stream
        if (!this->streamWasOpen)
        {
            this->stream->Close();
        }

        // set the current node to the root node
        this->curNode = this->xmlDocument->RootElement();
		if (this->curNode)
		{
			return true;
		} 
    }
    return false;
}

//------------------------------------------------------------------------------
/**
*/
void
XmlReader::Close()
{
    n_assert(0 != this->xmlDocument);

    // delete the xml document
    n_delete(this->xmlDocument);
    this->xmlDocument = 0;
    this->curNode = 0;

    StreamReader::Close();
}

//------------------------------------------------------------------------------
/**
    This method returns the line number of the current node.
*/
int
XmlReader::GetCurrentNodeLineNumber() const
{
    n_assert(this->curNode);
    return this->curNode->Row();
}

//------------------------------------------------------------------------------
/**
    This method finds an xml node by path name. It can handle absolute
    paths and paths relative to the current node. All the usual file system
    path conventions are valid: "/" is the path separator, "." is the
    current directory, ".." the parent directory.
*/
TiXmlNode*
XmlReader::FindNode(const String& path) const
{
    n_assert(this->IsOpen());
    n_assert(path.IsValid());

    bool absPath = (path[0] == '/');
    Array<String> tokens = path.Tokenize("/");

    // get starting node (either root or current node)
    TiXmlNode* node;
    if (absPath)
    {
        node = this->xmlDocument;
    }
    else
    {
        n_assert(0 != this->curNode);
        node = this->curNode;
    }

    // iterate through path components
    int i;
    int num = tokens.Size();
    for (i = 0; i < num; i++)
    {
        const String& cur = tokens[i];
        if ("." == cur)
        {
            // do nothing
        }
        else if (".." == cur)
        {
            // go to parent directory
            node = node->Parent();
            if (node == this->xmlDocument)
            {
				SYS_EXCEPT(Exceptions::FormatException, 
					STRING_FORMAT("path,that is %s, points above root node.", path.AsCharPtr()),
					GET_FUNCTION_NAME()
					);
                return 0;
            }
        }
        else
        {
            // find child node
            node = node->FirstChild(cur.AsCharPtr());
            if (0 == node)
            {
                return 0;
            }
        }
    }
    return node;
}

//------------------------------------------------------------------------------
/**
    This method returns true if the node identified by path exists. Path
    follows the normal filesystem path conventions, "/" is the separator,
    ".." is the parent node, "." is the current node. An absolute path
    starts with a "/", a relative path doesn't.
*/
bool
XmlReader::HasNode(const String& n) const
{
    return (this->FindNode(n) != 0);
}

//------------------------------------------------------------------------------
/**
    Get the short name (without path) of the current node.
*/
String
XmlReader::GetCurrentNodeName() const
{
    n_assert(this->IsOpen());
    n_assert(0 != this->curNode);
    return String(this->curNode->Value());
}

//------------------------------------------------------------------------------
/**
    This returns the full absolute path of the current node. Path components
    are separated by slashes.
*/
String
XmlReader::GetCurrentNodePath() const
{
    n_assert(this->IsOpen());
    n_assert(0 != this->curNode);

    // build bottom-up array of node names
    Array<String> components;
    TiXmlNode* node = this->curNode;
    while (node != this->xmlDocument)
    {
        components.Append(node->Value());
        node = node->Parent();
    }

    // build top down path
    String path = "/";
    int i;
    for (i = components.Size() - 1; i >= 0; --i)
    {
        path.Append(components[i]);
        if (i > 0)
        {
            path.Append("/");
        }
    }
    return path;
}

//------------------------------------------------------------------------------
/**
    Set the node pointed to by the path string as current node. The path
    may be absolute or relative, following the usual filesystem path 
    conventions. Separator is a slash.
*/
void
XmlReader::SetToNode(const String& path)
{
    n_assert(this->IsOpen());
    n_assert(path.IsValid());
    TiXmlNode* n = this->FindNode(path);
    if (n)
    {
        this->curNode = n->ToElement();
    }
    else
    {
		SYS_EXCEPT(Exceptions::FormatException, 
			STRING_FORMAT("node,that is %s, is not found.", path.AsCharPtr()),
			GET_FUNCTION_NAME()
			);
    }
}

//------------------------------------------------------------------------------
/**
    Sets the current node to the first child node. If no child node exists,
    the current node will remain unchanged and the method will return false.
    If name is a valid string, only child element matching the name will 
    be returned. If name is empty, all child nodes will be considered.
*/
bool
XmlReader::SetToFirstChild(const String& name)
{
    n_assert(this->IsOpen());
    n_assert(0 != this->curNode);
    TiXmlElement* child = 0;
    if (name.IsEmpty())
    {
        child = this->curNode->FirstChildElement();
    }
    else
    {
        child = this->curNode->FirstChildElement(name.AsCharPtr());
    }
    if (child)
    {
        this->curNode = child;
        return true;
    }
    else
    {
        return false;
    }
}

//------------------------------------------------------------------------------
/**
    Sets the current node to the next sibling. If no more children exist,
    the current node will be reset to the parent node and the method will 
    return false. If name is a valid string, only child element matching the 
    name will be returned. If name is empty, all child nodes will be considered.
*/
bool
XmlReader::SetToNextChild(const String& name)
{
    n_assert(this->IsOpen());
    n_assert(0 != this->curNode);

    TiXmlElement* sib = 0;
    if (name.IsEmpty())
    {
        sib = this->curNode->NextSiblingElement();
    }
    else
    {
        sib = this->curNode->NextSiblingElement(name.AsCharPtr());
    }
    if (sib)
    {
        this->curNode = sib;
        return true;
    }
    else
    {
        this->SetToParent();
        return false;
    }
}

//------------------------------------------------------------------------------
/**
    Sets the current node to its parent. If no parent exists, the
    current node will remain unchanged and the method will return false.
*/
bool
XmlReader::SetToParent()
{
    n_assert(this->IsOpen());
    n_assert(0 != this->curNode);
    TiXmlNode* parent = this->curNode->Parent();
    if (parent)
    {
        this->curNode = parent->ToElement();
        return true;
    }
    else
    {
        return false;
    }
}

//------------------------------------------------------------------------------
/**
    Return true if an attribute of the given name exists on the current node.
*/
bool
XmlReader::HasAttr(const char* name) const
{
    n_assert(this->IsOpen());

	if (!this->curNode)
	{
		n_warning("XmlReader::HasAttr(): failed to open %s", this->stream->GetURI().AsString().AsCharPtr());
		return false;
	}

    n_assert(0 != name);
    return (0 != this->curNode->Attribute(name));
}

//------------------------------------------------------------------------------
/**
    Return array with names of all attrs on current node
*/
Array<String>
XmlReader::GetAttrs() const
{
    n_assert(this->IsOpen());
    n_assert(0 != this->curNode);
    Array<String> res;
    const TiXmlAttribute * attr = this->curNode->FirstAttribute();
    while(0 != attr)
    {
        res.Append(attr->Name());
        attr = attr->Next();
    }
    return res;
}

//------------------------------------------------------------------------------
/**
    Return the provided attribute as string. If the attribute does not exist
    the method will fail hard (use HasAttr() to check for its existance).
*/
String
XmlReader::GetString(const char* name) const
{
    n_assert(this->IsOpen());
    n_assert(0 != this->curNode);
    n_assert(0 != name);
    String str;
    const char* val = this->curNode->Attribute(name);    
    if (0 == val)
    {
		SYS_EXCEPT(Exceptions::FormatException, 
			STRING_FORMAT("attribute '%s' doesn't exist on node '%s'!", name, this->curNode->Value()),
			GET_FUNCTION_NAME()
			);
    }
    else
    {
        str = val;
    }
    return str;
}
//------------------------------------------------------------------------------
/**
    Return the provided attribute as char*. If the attribute does not exist
    the method will fail hard (use HasAttr() to check for its existance).
*/
// - this function get one attribute and go to next attribute if exist.
const char* XmlReader::GetString_forward( const char* name )
{
	n_assert(this->IsOpen());
	n_assert(0 != this->curNode);
	n_assert(0 != name);
	
	const char* val = this->curNode->Attribute(name);    
	if (0 == val)
	{
		SYS_EXCEPT(Exceptions::FormatException, 
			STRING_FORMAT("attribute '%s' doesn't exist on node '%s'!", name, this->curNode->Value()),
			GET_FUNCTION_NAME()
			);
	}
	curNode = curNode->NextSiblingElement();
	return val;
}
//------------------------------------------------------------------------------
/**
    Return the provided attribute as a bool. If the attribute does not exist
    the method will fail hard (use HasAttr() to check for its existance).
*/
bool
XmlReader::GetBool(const char* name) const
{
    return this->GetString(name).AsBool();
}

//------------------------------------------------------------------------------
/**
    Return the provided attribute as int. If the attribute does not exist
    the method will fail hard (use HasAttr() to check for its existance).
*/
int
XmlReader::GetInt(const char* name) const
{
    return this->GetString(name).AsInt();
}

//------------------------------------------------------------------------------
/**
    Return the provided attribute as float. If the attribute does not exist
    the method will fail hard (use HasAttr() to check for its existance).
*/
float
XmlReader::GetFloat(const char* name) const
{
    return this->GetString(name).AsFloat();
}

  
//------------------------------------------------------------------------------
/**
    Return the provided attribute as float2. If the attribute does not exist
    the method will fail hard (use HasAttr() to check for its existance).
*/
float2
XmlReader::GetFloat2(const char* name) const
{
    return this->GetString(name).AsFloat2();
}

//------------------------------------------------------------------------------
/**
    Return the provided attribute as float4. If the attribute does not exist
    the method will fail hard (use HasAttr() to check for its existance).
*/
float4
XmlReader::GetFloat4(const char* name) const
{
#if NEBULA3_XMLREADER_LEGACY_VECTORS
    const String float4String = this->GetString(name);
    Array<String> tokens = float4String.Tokenize(", \t");
    if (tokens.Size() == 3)
    {
        return float4(tokens[0].AsFloat(), tokens[1].AsFloat(), tokens[2].AsFloat(), 0);
    }
	else if (tokens.Size() == 4)
	{
		return float4(tokens[0].AsFloat(), tokens[1].AsFloat(), tokens[2].AsFloat(), tokens[3].AsFloat());
	}
	else
	{
		SYS_EXCEPT(Exceptions::FormatException, 
			STRING_FORMAT("invalid string value for float4: %s.", float4String.AsCharPtr()),
			GET_FUNCTION_NAME()
			);
	}
#else
    return this->GetString(name).AsFloat4();
#endif

}

//------------------------------------------------------------------------------
/**
    Return the provided attribute as float3. If the attribute does not exist
    the method will fail hard (use HasAttr() to check for its existance).
*/
float3
XmlReader::GetFloat3(const char* name) const
{
    return this->GetString(name).AsFloat3();
}
//------------------------------------------------------------------------------
/**
    Return the provided attribute as matrix44. If the attribute does not exist
    the method will fail hard (use HasAttr() to check for its existance).
*/
matrix44
XmlReader::GetMatrix44(const char* name) const
{
    return this->GetString(name).AsMatrix44();
}


Util::AssetPath XmlReader::GetAssetPath( const char* name ) const
{
	return this->GetString(name).AsAssetPath();
}
//------------------------------------------------------------------------------
/**
    Return the provided optional attribute as string. If the attribute doesn't
    exist, the default value will be returned.
*/
String
XmlReader::GetOptString(const char* name, const String& defaultValue) const
{
    if (this->HasAttr(name))
    {
        return this->GetString(name);
    }
    else
    {
        return defaultValue;
    }
}

//------------------------------------------------------------------------------
/**
    Return the provided optional attribute as bool. If the attribute doesn't
    exist, the default value will be returned.
*/
bool
XmlReader::GetOptBool(const char* name, bool defaultValue) const
{
    if (this->HasAttr(name))
    {
        return this->GetBool(name);
    }
    else
    {
        return defaultValue;
    }
}

//------------------------------------------------------------------------------
/**
    Return the provided optional attribute as int. If the attribute doesn't
    exist, the default value will be returned.
*/
int
XmlReader::GetOptInt(const char* name, int defaultValue) const
{
    if (this->HasAttr(name))
    {
        return this->GetInt(name);
    }
    else
    {
        return defaultValue;
    }
}

//------------------------------------------------------------------------------
/**
    Return the provided optional attribute as float. If the attribute doesn't
    exist, the default value will be returned.
*/
float
XmlReader::GetOptFloat(const char* name, float defaultValue) const
{
    if (this->HasAttr(name))
    {
        return this->GetFloat(name);
    }
    else
    {
        return defaultValue;
    }
}

  
//------------------------------------------------------------------------------
/**
    Return the provided optional attribute as float2. If the attribute doesn't
    exist, the default value will be returned.
*/
float2
XmlReader::GetOptFloat2(const char* name, const float2& defaultValue) const
{
    if (this->HasAttr(name))
    {
        return this->GetFloat2(name);
    }
    else
    {
        return defaultValue;
    }
}

//------------------------------------------------------------------------------
/**
    Return the provided optional attribute as float4. If the attribute doesn't
    exist, the default value will be returned.
*/
float4
XmlReader::GetOptFloat4(const char* name, const float4& defaultValue) const
{
    if (this->HasAttr(name))
    {
        return this->GetFloat4(name);
    }
    else
    {
        return defaultValue;
    }
}

//------------------------------------------------------------------------------
/**
    Return the provided optional attribute as matrix44. If the attribute doesn't
    exist, the default value will be returned.
*/
matrix44
XmlReader::GetOptMatrix44(const char* name, const matrix44& defaultValue) const
{
    if (this->HasAttr(name))
    {
        return this->GetMatrix44(name);
    }
    else
    {
        return defaultValue;
    }
}

    
//------------------------------------------------------------------------------
/**
*/
bool
XmlReader::HasContent() const
{
    n_assert(this->IsOpen());
    n_assert(0 != this->curNode);
    TiXmlNode* child = this->curNode->FirstChild();
    return child && (child->Type() == TiXmlNode::TEXT);
}

//------------------------------------------------------------------------------
/**
*/
String
XmlReader::GetContent() const
{
    n_assert(this->IsOpen());
    n_assert(this->curNode);
    TiXmlNode* child = this->curNode->FirstChild();
    n_assert(child->Type() == TiXmlNode::TEXT);
    return child->Value();
}

} // namespace IO