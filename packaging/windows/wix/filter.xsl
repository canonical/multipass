<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0" exclude-result-prefixes="xsl wix"
                xmlns:wix="http://wixtoolset.org/schemas/v4/wxs"
                xmlns="http://wixtoolset.org/schemas/v4/wxs">

    <xsl:output method="xml" indent="yes" omit-xml-declaration="yes" />

    <xsl:strip-space elements="*" />

    <xsl:key name="FilterMultipassd" match="wix:Component[wix:File[contains(@Source, 'multipassd.exe')]]" use="@Id" />
    <xsl:key name="FilterMultipass" match="wix:Component[wix:File[contains(@Source, 'multipass.exe')]]" use="@Id" />
    <xsl:key name="FilterMultipassGUI" match="wix:Component[wix:File[contains(@Source, 'multipass.gui.exe')]]" use="@Id" />
    <xsl:key name="FilterFontDir" match="wix:Component[contains(wix:File/@Source, '\fonts\')]" use="@Id" />

    <!-- Copy all elements and their attributes. -->
    <xsl:template match="@*|node()">
        <xsl:copy>
            <xsl:apply-templates select="@*|node()" />
        </xsl:copy>
    </xsl:template>

    <!-- Except for those that match our filters, do nothing. -->
    <xsl:template match="*[ self::wix:Component or self::wix:ComponentRef ][ key( 'FilterMultipassd', @Id ) ]" />
    <xsl:template match="*[ self::wix:Component or self::wix:ComponentRef ][ key( 'FilterMultipass', @Id ) ]" />
    <xsl:template match="*[ self::wix:Component or self::wix:ComponentRef ][ key( 'FilterMultipassGUI', @Id ) ]" />
    <xsl:template match="*[ self::wix:Component or self::wix:ComponentRef ][ key( 'FilterFontDir', @Id ) ]" />
</xsl:stylesheet>
