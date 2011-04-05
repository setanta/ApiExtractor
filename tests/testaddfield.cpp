/*
* This file is part of the API Extractor project.
*
* Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
*
* Contact: PySide team <contact@pyside.org>
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
* 02110-1301 USA
*
*/

#include "testaddfield.h"
#include <QtTest/QTest>
#include "testutil.h"


void TestAddField::testAddFieldToClass()
{
    const char cppCode[] = "struct A {};";
    const char xmlCode[] = "\
    <typesystem package='Foo'>\
        <primitive-type name='double'/> \
        <value-type name='A'>\
            <add-field name='addedFloat' type='double'>12.3</add-field>\
        </value-type>\
    </typesystem>";
    TestUtil t(cppCode, xmlCode);
    AbstractMetaClassList classes = t.builder()->classes();
    AbstractMetaClass* classA = classes.findClass("A");
    QVERIFY(classA);
    QCOMPARE(classA->fields().count(), 1);

    AbstractMetaField* addedField = classA->fields().last();
    QVERIFY(addedField);
    QVERIFY(addedField->isUserAdded());
    QCOMPARE(addedField->enclosingClass(), classA);
    QCOMPARE(addedField->name(), QString("addedFloat"));
    QCOMPARE(addedField->type()->cppSignature(), QString("double"));
    QCOMPARE(addedField->defaultValueExpression(), QString("12.3"));
}

void TestAddField::testAddFieldToNamespace()
{
    const char cppCode[] = "\
    namespace Namespace {\
        struct A {};\
    }\
    ";
    const char xmlCode[] = "\
    <typesystem package='Foo'>\
        <namespace-type name='Namespace'>\
            <value-type name='A' />\
            <add-field name='addedObject' type='Namespace::A'>Namespace::A()</add-field>\
        </namespace-type>\
    </typesystem>";
    TestUtil t(cppCode, xmlCode);
    AbstractMetaClassList classes = t.builder()->classes();
    AbstractMetaClass* nspace = classes.findClass("Namespace");
    QVERIFY(nspace);
    QCOMPARE(nspace->fields().count(), 1);

    AbstractMetaField* addedField = nspace->fields().first();
    QVERIFY(addedField);
    QVERIFY(addedField->isUserAdded());
    QCOMPARE(addedField->enclosingClass(), nspace);
    QCOMPARE(addedField->name(), QString("addedObject"));
    QCOMPARE(addedField->type()->cppSignature(), QString("Namespace::A"));
    QCOMPARE(addedField->defaultValueExpression(), QString("Namespace::A()"));

    AbstractMetaClass* classA = classes.findClass("Namespace::A");
    QVERIFY(classA);
    QCOMPARE(classA->typeEntry(), addedField->type()->typeEntry());
}

void TestAddField::testAddIntFieldAtModuleLevel()
{
    const char cppCode[] = "";
    const char xmlCode[] = "\
    <typesystem package='Foo'>\
        <primitive-type name='int'/> \
        <add-field name='addedInt' type='int'>123</add-field>\
    </typesystem>";
    TestUtil t(cppCode, xmlCode);

    AbstractMetaFieldList globalFields = t.builder()->globalFields();
    QCOMPARE(globalFields.count(), 1);
    AbstractMetaField* addedField = globalFields.first();
    QVERIFY(addedField);
    QVERIFY(addedField->isUserAdded());
    QVERIFY(!addedField->enclosingClass());
    QCOMPARE(addedField->name(), QString("addedInt"));
    QCOMPARE(addedField->type()->cppSignature(), QString("int"));
    QCOMPARE(addedField->defaultValueExpression(), QString("123"));
}

void TestAddField::testAddStringFieldAtModuleLevel()
{
    const char cppCode[] = "";
    const char xmlCode[] = "\
    <typesystem package='Foo'>\
        <primitive-type name='int'/> \
        <primitive-type name='char'/> \
        <add-field name='stringLiteral' type='const char*'>\
        \"foobar\"\
        </add-field>\
        <add-field name='stringFunction' type='const char*'>\
        returnString()\
        </add-field>\
    </typesystem>";
    TestUtil t(cppCode, xmlCode);

    AbstractMetaFieldList globalFields = t.builder()->globalFields();
    QCOMPARE(globalFields.count(), 2);

    AbstractMetaField* stringField0 = globalFields.first();
    QVERIFY(stringField0);
    QVERIFY(stringField0->isUserAdded());
    QVERIFY(!stringField0->enclosingClass());
    QCOMPARE(stringField0->name(), QString("stringLiteral"));
    QCOMPARE(stringField0->type()->cppSignature(), QString("const char *"));
    QCOMPARE(stringField0->defaultValueExpression(), QString("\"foobar\""));

    AbstractMetaField* stringField1 = globalFields.last();
    QVERIFY(stringField1);
    QVERIFY(stringField1->isUserAdded());
    QVERIFY(!stringField1->enclosingClass());
    QCOMPARE(stringField1->name(), QString("stringFunction"));
    QCOMPARE(stringField1->type()->cppSignature(), QString("const char *"));
    QCOMPARE(stringField1->defaultValueExpression(), QString("returnString()"));
}

void TestAddField::testAddObjectFieldAtModuleLevel()
{
    const char cppCode[] = "struct A {};";
    const char xmlCode[] = "\
    <typesystem package='Foo'>\
        <value-type name='A'/> \
        <add-field name='addedObject' type='A'>A()</add-field>\
    </typesystem>";
    TestUtil t(cppCode, xmlCode);

    AbstractMetaFieldList globalFields = t.builder()->globalFields();
    QCOMPARE(globalFields.count(), 1);
    AbstractMetaField* addedField = globalFields.first();
    QVERIFY(addedField);
    QVERIFY(addedField->isUserAdded());
    QVERIFY(!addedField->enclosingClass());
    QCOMPARE(addedField->name(), QString("addedObject"));
    QCOMPARE(addedField->type()->cppSignature(), QString("A"));
    QCOMPARE(addedField->defaultValueExpression(), QString("A()"));
}

QTEST_APPLESS_MAIN(TestAddField)

#include "testaddfield.moc"

