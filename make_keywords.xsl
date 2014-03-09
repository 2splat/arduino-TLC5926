<?xml version="1.0"?>
<!DOCTYPE xsl:stylesheet [
    <!ENTITY eol '<text xmlns="http://www.w3.org/1999/XSL/Transform">
</text>'>
    <!ENTITY sp '<text xmlns="http://www.w3.org/1999/XSL/Transform"> </text>'>
    ]>

<xsl:stylesheet version='1.0' xmlns:xsl='http://www.w3.org/1999/XSL/Transform'
        >
    <xsl:output  method="text"/>

<!--
    \-\-stringparam class_name $classnameofinterest
-->
<xsl:param name="class_name" value=""/>

<xsl:template match="//Class">
        <xsl:if test="@name=$class_name">
            <xsl:value-of select="@name" /><xsl:text> KEYWORD1</xsl:text>&eol;
            <xsl:variable name="class_id" select="@id"/>
            <xsl:apply-templates select="//Method[@context=$class_id]" mode="doclass"/>
        </xsl:if>
</xsl:template>

<xsl:template match="Method[@access='public']" mode="doclass">
    <xsl:value-of select="@name" /><xsl:text> KEYWORD2</xsl:text>&eol;
</xsl:template>

<xsl:template match="node()" mode="doclass"/>

<xsl:template match="@*|node()">
        <xsl:apply-templates select="@*|node()"/>
</xsl:template>



</xsl:stylesheet>
