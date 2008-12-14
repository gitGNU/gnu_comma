<?xml version='1.0'?>

<!-- This file is distributed under the MIT License.  See LICENSE.txt for
     details.

     Copyright 2008, Stephen Wilson
     -->

<xsl:stylesheet
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">

<xsl:import href="http://docbook.sourceforge.net/release/xsl/current/html/chunk.xsl"/>

<xsl:param name="use.extensions">0</xsl:param>
<xsl:param name="chunk.section.depth">0</xsl:param>
<xsl:param name="section.autolabel">1</xsl:param>
<xsl:param name="section.label.includes.component.label">1</xsl:param>
<xsl:param name="ebnf.table.bgcolor">#F0F8FF</xsl:param>
<xsl:param name="ebnf.table.border">1</xsl:param>

<xsl:param name="local.l10n.xml" select="document('')"/>
<l:i18n xmlns:l="http://docbook.sourceforge.net/xmlns/l10n/1.0">
   <l:l10n language="en">
     <l:context name="xref-number-and-title">
       <l:template name="sect1"   style="sect" text="(&#167; %n)"/>
       <l:template name="sect2"   style="sect" text="(&#167; %n)"/>
       <l:template name="sect3"   style="sect" text="(&#167; %n)"/>
       <l:template name="sect4"   style="sect" text="(&#167; %n)"/>
       <l:template name="sect5"   style="sect" text="(&#167; %n)"/>
       <l:template name="section" style="sect" text="(&#167; %n)"/>
     </l:context>
   </l:l10n>
</l:i18n>

<xsl:param name="html.stylesheet">commaspec.css</xsl:param>


</xsl:stylesheet>
