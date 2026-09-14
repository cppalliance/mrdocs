<?xml version='1.0' encoding='UTF-8' standalone='yes' ?>
<tagfile doxygen_version="1.14.0" doxygen_gitid="cbe58f6237b2238c9af7f51c6b7afb8bbf52c866">
  <compound kind="file">
    <name>geomlib.hpp</name>
    <path></path>
    <filename>geomlib_8hpp.html</filename>
    <class kind="class">geom::shape</class>
    <class kind="class">geom::circle</class>
    <class kind="struct">geom::box</class>
    <namespace>geom</namespace>
  </compound>
  <compound kind="struct">
    <name>geom::box</name>
    <filename>structgeom_1_1box.html</filename>
    <templarg>int N</templarg>
    <member kind="function">
      <type>double</type>
      <name>volume</name>
      <anchorfile>structgeom_1_1box.html</anchorfile>
      <anchor>a6e4ee73e4f34c52b03baedf302df2a4b</anchor>
      <arglist>() const</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>geom::circle</name>
    <filename>classgeom_1_1circle.html</filename>
    <base>geom::shape</base>
    <member kind="function">
      <type></type>
      <name>circle</name>
      <anchorfile>classgeom_1_1circle.html</anchorfile>
      <anchor>a2e28dd7289ac31337600423f096be16c</anchor>
      <arglist>(double r)</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>area</name>
      <anchorfile>classgeom_1_1circle.html</anchorfile>
      <anchor>aaf7d048e35f1058e63a3c50086b5b35c</anchor>
      <arglist>() const override</arglist>
    </member>
    <member kind="variable">
      <type>double</type>
      <name>radius</name>
      <anchorfile>classgeom_1_1circle.html</anchorfile>
      <anchor>af446fa74ba50176eff2fb57e9bc03755</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>geom::shape</name>
    <filename>classgeom_1_1shape.html</filename>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~shape</name>
      <anchorfile>classgeom_1_1shape.html</anchorfile>
      <anchor>a02240e2b29d285fd3c7198c555766ee4</anchor>
      <arglist>()=default</arglist>
    </member>
    <member kind="function" virtualness="pure">
      <type>virtual double</type>
      <name>area</name>
      <anchorfile>classgeom_1_1shape.html</anchorfile>
      <anchor>ae85810a772ca5f9570e2dd05d105cfc8</anchor>
      <arglist>() const =0</arglist>
    </member>
  </compound>
  <compound kind="namespace">
    <name>geom</name>
    <filename>namespacegeom.html</filename>
    <class kind="struct">geom::box</class>
    <class kind="class">geom::circle</class>
    <class kind="class">geom::shape</class>
    <member kind="typedef">
      <type>double</type>
      <name>real</name>
      <anchorfile>namespacegeom.html</anchorfile>
      <anchor>a29072f2596108a05c3dc96b5c7813481</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>fill</name>
      <anchorfile>namespacegeom.html</anchorfile>
      <anchor>ab1bb1819938e7e7575623165481eb85a</anchor>
      <arglist></arglist>
      <enumvalue file="namespacegeom.html" anchor="ab1bb1819938e7e7575623165481eb85aaec03f91ae56e478455e3786e91559194">solid</enumvalue>
      <enumvalue file="namespacegeom.html" anchor="ab1bb1819938e7e7575623165481eb85aaff362afb00fa83da92fd95b38424a556">hollow</enumvalue>
    </member>
    <member kind="function">
      <type>double</type>
      <name>pi</name>
      <anchorfile>namespacegeom.html</anchorfile>
      <anchor>aaaf1b9ae47d530152a0380e337c5e2ff</anchor>
      <arglist>()</arglist>
    </member>
  </compound>
</tagfile>
