/*
 * This file is part of the API Extractor project.
 *
 * Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies).
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

#include "typesystem.h"
#include "typesystem_p.h"
#include "typedatabase.h"
#include "reporthandler.h"
#include <QtXml>

static QString strings_Object = QLatin1String("Object");
static QString strings_String = QLatin1String("String");
static QString strings_char = QLatin1String("char");
static QString strings_jchar = QLatin1String("jchar");
static QString strings_jobject = QLatin1String("jobject");

Handler::Handler(TypeDatabase* database, bool generate)
            : m_database(database), m_generate(generate ? TypeEntry::GenerateAll : TypeEntry::GenerateForSubclass)
{
    m_currentEnum = 0;
    m_current = 0;

    tagNames["rejection"] = StackElement::Rejection;
    tagNames["primitive-type"] = StackElement::PrimitiveTypeEntry;
    tagNames["container-type"] = StackElement::ContainerTypeEntry;
    tagNames["object-type"] = StackElement::ObjectTypeEntry;
    tagNames["value-type"] = StackElement::ValueTypeEntry;
    tagNames["interface-type"] = StackElement::InterfaceTypeEntry;
    tagNames["namespace-type"] = StackElement::NamespaceTypeEntry;
    tagNames["enum-type"] = StackElement::EnumTypeEntry;
    tagNames["function"] = StackElement::FunctionTypeEntry;
    tagNames["extra-includes"] = StackElement::ExtraIncludes;
    tagNames["include"] = StackElement::Include;
    tagNames["inject-code"] = StackElement::InjectCode;
    tagNames["modify-function"] = StackElement::ModifyFunction;
    tagNames["modify-field"] = StackElement::ModifyField;
    tagNames["access"] = StackElement::Access;
    tagNames["remove"] = StackElement::Removal;
    tagNames["rename"] = StackElement::Rename;
    tagNames["typesystem"] = StackElement::Root;
    tagNames["custom-constructor"] = StackElement::CustomMetaConstructor;
    tagNames["custom-destructor"] = StackElement::CustomMetaDestructor;
    tagNames["argument-map"] = StackElement::ArgumentMap;
    tagNames["suppress-warning"] = StackElement::SuppressedWarning;
    tagNames["load-typesystem"] = StackElement::LoadTypesystem;
    tagNames["define-ownership"] = StackElement::DefineOwnership;
    tagNames["replace-default-expression"] = StackElement::ReplaceDefaultExpression;
    tagNames["reject-enum-value"] = StackElement::RejectEnumValue;
    tagNames["replace-type"] = StackElement::ReplaceType;
    tagNames["conversion-rule"] = StackElement::ConversionRule;
    tagNames["modify-argument"] = StackElement::ModifyArgument;
    tagNames["remove-argument"] = StackElement::RemoveArgument;
    tagNames["remove-default-expression"] = StackElement::RemoveDefaultExpression;
    tagNames["template"] = StackElement::Template;
    tagNames["insert-template"] = StackElement::TemplateInstanceEnum;
    tagNames["replace"] = StackElement::Replace;
    tagNames["no-null-pointer"] = StackElement::NoNullPointers;
    tagNames["reference-count"] = StackElement::ReferenceCount;
    tagNames["parent"] = StackElement::ParentOwner;
    tagNames["inject-documentation"] = StackElement::InjectDocumentation;
    tagNames["modify-documentation"] = StackElement::ModifyDocumentation;
    tagNames["add-function"] = StackElement::AddFunction;
}

bool Handler::error(const QXmlParseException &e)
{
    qWarning("Error: line=%d, column=%d, message=%s\n",
             e.lineNumber(), e.columnNumber(), qPrintable(e.message()));
    return false;
}

bool Handler::fatalError(const QXmlParseException &e)
{
    qWarning("Fatal error: line=%d, column=%d, message=%s\n",
             e.lineNumber(), e.columnNumber(), qPrintable(e.message()));

    return false;
}

bool Handler::warning(const QXmlParseException &e)
{
    qWarning("Warning: line=%d, column=%d, message=%s\n",
             e.lineNumber(), e.columnNumber(), qPrintable(e.message()));

    return false;
}

void Handler::fetchAttributeValues(const QString &name, const QXmlAttributes &atts,
                                   QHash<QString, QString> *acceptedAttributes)
{
    Q_ASSERT(acceptedAttributes);

    for (int i = 0; i < atts.length(); ++i) {
        QString key = atts.localName(i).toLower();
        QString val = atts.value(i);

        if (!acceptedAttributes->contains(key))
            ReportHandler::warning(QString("Unknown attribute for '%1': '%2'").arg(name).arg(key));
         else
            (*acceptedAttributes)[key] = val;

    }
}

bool Handler::endElement(const QString &, const QString &localName, const QString &)
{
    QString tagName = localName.toLower();
    if (tagName == "import-file")
        return true;

    if (!m_current)
        return true;

    switch (m_current->type) {
    case StackElement::AddFunction:
        if (m_generate == TypeEntry::GenerateAll
            && m_current->parent
            && m_current->parent->type == StackElement::Root) { // Global function
            TypeDatabase::instance()->addGlobalUserFunctions(m_addedFunctions);
            TypeDatabase::instance()->addGlobalUserFunctionModifications(m_functionMods);
            m_addedFunctions.clear();
            m_functionMods.clear();
        }
        break;
    case StackElement::ObjectTypeEntry:
    case StackElement::ValueTypeEntry:
    case StackElement::InterfaceTypeEntry:
    case StackElement::NamespaceTypeEntry: {
        ComplexTypeEntry *centry = static_cast<ComplexTypeEntry *>(m_current->entry);
        centry->setAddedFunctions(m_addedFunctions);
        centry->setFunctionModifications(m_functionMods);
        centry->setFieldModifications(m_fieldMods);
        centry->setCodeSnips(m_codeSnips);
        centry->setDocModification(m_docModifications);

        if (centry->designatedInterface()) {
            centry->designatedInterface()->setCodeSnips(m_codeSnips);
            centry->designatedInterface()->setFunctionModifications(m_functionMods);
        }
        m_codeSnips = CodeSnipList();
        m_addedFunctions = AddedFunctionList();
        m_functionMods = FunctionModificationList();
        m_fieldMods = FieldModificationList();
        m_docModifications = DocModificationList();
    }
    break;
    case StackElement::CustomMetaConstructor: {
        m_current->entry->setCustomConstructor(*m_current->value.customFunction);
        delete m_current->value.customFunction;
    }
    break;
    case StackElement::CustomMetaDestructor: {
        m_current->entry->setCustomDestructor(*m_current->value.customFunction);
        delete m_current->value.customFunction;
    }
    break;
    case StackElement::EnumTypeEntry:
        m_current->entry->setDocModification(m_docModifications);
        m_docModifications = DocModificationList();
        m_currentEnum = 0;
        break;
    case StackElement::Template:
        m_database->addTemplate(m_current->value.templateEntry);
        break;
    case StackElement::TemplateInstanceEnum:
        if (m_current->parent->type == StackElement::InjectCode)
            m_codeSnips.last().addTemplateInstance(m_current->value.templateInstance);
         else if (m_current->parent->type == StackElement::Template)
            m_current->parent->value.templateEntry->addTemplateInstance(m_current->value.templateInstance);
         else if (m_current->parent->type == StackElement::CustomMetaConstructor
                  || m_current->parent->type == StackElement::CustomMetaConstructor)
            m_current->parent->value.customFunction->addTemplateInstance(m_current->value.templateInstance);
         else if (m_current->parent->type == StackElement::ConversionRule)
            m_functionMods.last().argument_mods.last().conversion_rules.last().addTemplateInstance(m_current->value.templateInstance);
         else if (m_current->parent->type == StackElement::InjectCodeInFunction)
            m_functionMods.last().snips.last().addTemplateInstance(m_current->value.templateInstance);

        break;
    default:
        break;
    }

    StackElement *child = m_current;
    m_current = m_current->parent;
    delete(child);

    return true;
}

bool Handler::characters(const QString &ch)
{
    if (m_current->type == StackElement::Template) {
        m_current->value.templateEntry->addCode(ch);
        return true;
    }

    if (m_current->type == StackElement::CustomMetaConstructor || m_current->type == StackElement::CustomMetaDestructor) {
        m_current->value.customFunction->addCode(ch);
        return true;
    }

    if (m_current->type == StackElement::ConversionRule
        && m_current->parent->type == StackElement::ModifyArgument) {
        m_functionMods.last().argument_mods.last().conversion_rules.last().addCode(ch);
        return true;
    }

    if (m_current->parent) {
        if ((m_current->type & StackElement::CodeSnipMask)) {
            CodeSnipList snips;
            switch (m_current->parent->type) {
            case StackElement::Root:
                snips = m_current->parent->entry->codeSnips();
                snips.last().addCode(ch);
                m_current->parent->entry->setCodeSnips(snips);
                break;
            case StackElement::ModifyFunction:
            case StackElement::AddFunction:
                m_functionMods.last().snips.last().addCode(ch);
                m_functionMods.last().modifiers |= FunctionModification::CodeInjection;
                break;
            case StackElement::NamespaceTypeEntry:
            case StackElement::ObjectTypeEntry:
            case StackElement::ValueTypeEntry:
            case StackElement::InterfaceTypeEntry:
                m_codeSnips.last().addCode(ch);
                break;
            default:
                Q_ASSERT(false);
            };
            return true;
        }
    }

    if (m_current->type & StackElement::DocumentationMask)
        m_docModifications.last().setCode(ch);

    return true;
}

bool Handler::importFileElement(const QXmlAttributes &atts)
{
    QString fileName = atts.value("name");
    if (fileName.isEmpty()) {
        m_error = "Required attribute 'name' missing for include-file tag.";
        return false;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        file.setFileName(":/trolltech/generator/" + fileName);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            m_error = QString("Could not open file: '%1'").arg(fileName);
            return false;
        }
    }

    QString quoteFrom = atts.value("quote-after-line");
    bool foundFromOk = quoteFrom.isEmpty();
    bool from = quoteFrom.isEmpty();

    QString quoteTo = atts.value("quote-before-line");
    bool foundToOk = quoteTo.isEmpty();
    bool to = true;

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (from && to && line.contains(quoteTo)) {
            to = false;
            foundToOk = true;
            break;
        }
        if (from && to)
            characters(line + "\n");
        if (!from && line.contains(quoteFrom)) {
            from = true;
            foundFromOk = true;
        }
    }
    if (!foundFromOk || !foundToOk) {
        QString fromError = QString("Could not find quote-after-line='%1' in file '%2'.").arg(quoteFrom).arg(fileName);
        QString toError = QString("Could not find quote-before-line='%1' in file '%2'.").arg(quoteTo).arg(fileName);

        if (!foundToOk)
            m_error = toError;
        if (!foundFromOk)
            m_error = fromError;
        if (!foundFromOk && !foundToOk)
            m_error = fromError + " " + toError;
        return false;
    }

    return true;
}

bool Handler::convertBoolean(const QString &_value, const QString &attributeName, bool defaultValue)
{
    QString value = _value.toLower();
    if (value == "true" || value == "yes")
        return true;
     else if (value == "false" || value == "no")
        return false;
     else {
        QString warn = QString("Boolean value '%1' not supported in attribute '%2'. Use 'yes' or 'no'. Defaulting to '%3'.")
                       .arg(value).arg(attributeName).arg(defaultValue ? "yes" : "no");

        ReportHandler::warning(warn);
        return defaultValue;
    }
}

bool Handler::startElement(const QString &, const QString &n,
                           const QString &, const QXmlAttributes &atts)
{
    QString tagName = n.toLower();
    if (tagName == "import-file")
        return importFileElement(atts);


    StackElement *element = new StackElement(m_current);

    if (!tagNames.contains(tagName)) {
        m_error = QString("Unknown tag name: '%1'").arg(tagName);
        return false;
    }

    element->type = tagNames[tagName];
    if (element->type & StackElement::TypeEntryMask) {
        if (m_current->type != StackElement::Root) {
            m_error = "Nested types not supported";
            return false;
        }

        QHash<QString, QString> attributes;
        attributes["name"] = QString();
        attributes["since"] = QString("0");

        switch (element->type) {
        case StackElement::PrimitiveTypeEntry:
            attributes["target-lang-name"] = QString();
            attributes["target-lang-api-name"] = QString();
            attributes["preferred-conversion"] = "yes";
            attributes["preferred-target-lang-type"] = "yes";
            attributes["default-constructor"] = QString();
            break;
        case StackElement::ContainerTypeEntry:
            attributes["type"] = QString();
            break;
        case StackElement::EnumTypeEntry:
            attributes["flags"] = "no";
            attributes["upper-bound"] = QString();
            attributes["lower-bound"] = QString();
            attributes["force-integer"] = "no";
            attributes["extensible"] = "no";
            attributes["identified-by-value"] = QString();
            break;
        case StackElement::ObjectTypeEntry:
        case StackElement::ValueTypeEntry:
            attributes["force-abstract"] = QString("no");
            attributes["deprecated"] = QString("no");
            attributes["hash-function"] = QString("");
            attributes["stream"] = QString("no");
            // fall throooough
        case StackElement::InterfaceTypeEntry:
            attributes["default-superclass"] = m_defaultSuperclass;
            attributes["polymorphic-id-expression"] = QString();
            attributes["delete-in-main-thread"] = QString("no");
            attributes["held-type"] = QString();
            attributes["copyable"] = QString();
            // fall through
        case StackElement::NamespaceTypeEntry:
            attributes["target-lang-name"] = QString();
            attributes["package"] = m_defaultPackage;
            attributes["expense-cost"] = "1";
            attributes["expense-limit"] = "none";
            attributes["polymorphic-base"] = QString("no");
            attributes["generate"] = QString("yes");
            attributes["target-type"] = QString();
            attributes["generic-class"] = QString("no");
            break;
        case StackElement::FunctionTypeEntry:
            attributes["signature"] = QString();
            break;
        default:
            { } // nada
        };

        fetchAttributeValues(tagName, atts, &attributes);
        QString name = attributes["name"];
        double since = attributes["since"].toDouble();

        // The top level tag 'function' has only the 'signature' tag
        // and we should extract the 'name' value from it.
        if (element->type == StackElement::FunctionTypeEntry) {
            QString signature = attributes["signature"];
            name = signature.left(signature.indexOf('(')).trimmed();
        }

        // We need to be able to have duplicate primitive type entries,
        // or it's not possible to cover all primitive target language
        // types (which we need to do in order to support fake meta objects)
        if (element->type != StackElement::PrimitiveTypeEntry
            && element->type != StackElement::FunctionTypeEntry) {
            TypeEntry *tmp = m_database->findType(name);
            if (tmp)
                ReportHandler::warning(QString("Duplicate type entry: '%1'").arg(name));
        }

        if (element->type == StackElement::EnumTypeEntry) {
            if (name.isEmpty()) {
                name = attributes["identified-by-value"];
            } else if (!attributes["identified-by-value"].isEmpty()) {
                m_error = "can't specify both 'name' and 'identified-by-value' attributes";
                return false;
            }
        }

        if (name.isEmpty()) {
            m_error = "no 'name' attribute specified";
            return false;
        }
        switch (element->type) {
        case StackElement::PrimitiveTypeEntry: {
            QString targetLangName = attributes["target-lang-name"];
            QString targetLangApiName = attributes["target-lang-api-name"];
            QString preferredConversion = attributes["preferred-conversion"].toLower();
            QString preferredTargetLangType = attributes["preferred-target-lang-type"].toLower();
            QString defaultConstructor = attributes["default-constructor"];

            if (targetLangName.isEmpty())
                targetLangName = name;
            if (targetLangApiName.isEmpty())
                targetLangApiName = name;

            PrimitiveTypeEntry *type = new PrimitiveTypeEntry(name, since);
            type->setCodeGeneration(m_generate);
            type->setTargetLangName(targetLangName);
            type->setTargetLangApiName(targetLangApiName);
            type->setDefaultConstructor(defaultConstructor);

            bool preferred;
            preferred = convertBoolean(preferredConversion, "preferred-conversion", true);
            type->setPreferredConversion(preferred);
            preferred = convertBoolean(preferredTargetLangType,
                                       "preferred-target-lang-type", true);
            type->setPreferredTargetLangType(preferred);

            element->entry = type;
            }
            break;
        case StackElement::ContainerTypeEntry:
            {
                QString typeName = attributes["type"];
                ContainerTypeEntry::Type containerType =
                        ContainerTypeEntry::containerTypeFromString(typeName);
                if (typeName.isEmpty()) {
                    m_error = "no 'type' attribute specified";
                    return false;
                } else if (containerType == ContainerTypeEntry::NoContainer) {
                    m_error = "there is no container of type " + containerType;
                    return false;
                }

                ContainerTypeEntry *type = new ContainerTypeEntry(name, containerType, since);
                type->setCodeGeneration(m_generate);
                element->entry = type;
            }
            break;
        case StackElement::EnumTypeEntry: {
            QStringList names = name.split(QLatin1String("::"));

            if (names.size() == 1)
                m_currentEnum = new EnumTypeEntry(QString(), name, since);
             else
                m_currentEnum =
                    new EnumTypeEntry(QStringList(names.mid(0, names.size() - 1)).join("::"),
                                      names.last(), since);
            m_currentEnum->setAnonymous(!attributes["identified-by-value"].isEmpty());
            element->entry = m_currentEnum;
            m_currentEnum->setCodeGeneration(m_generate);
            m_currentEnum->setTargetLangPackage(m_defaultPackage);
            m_currentEnum->setUpperBound(attributes["upper-bound"]);
            m_currentEnum->setLowerBound(attributes["lower-bound"]);
            m_currentEnum->setForceInteger(convertBoolean(attributes["force-integer"], "force-integer", false));
            m_currentEnum->setExtensible(convertBoolean(attributes["extensible"], "extensible", false));

            // put in the flags parallel...
            if (!attributes["flags"].isEmpty() && attributes["flags"].toLower() != "no") {
                FlagsTypeEntry *ftype = new FlagsTypeEntry("QFlags<" + name + ">", since);
                ftype->setOriginator(m_currentEnum);
                ftype->setOriginalName(attributes["flags"]);
                ftype->setCodeGeneration(m_generate);
                QString n = ftype->originalName();

                QStringList lst = n.split("::");
                if (QStringList(lst.mid(0, lst.size() - 1)).join("::") != m_currentEnum->targetLangQualifier()) {
                    ReportHandler::warning(QString("enum %1 and flags %2 differ in qualifiers")
                                           .arg(m_currentEnum->targetLangQualifier())
                                           .arg(lst.at(0)));
                }

                ftype->setFlagsName(lst.last());
                m_currentEnum->setFlags(ftype);

                m_database->addFlagsType(ftype);
                m_database->addType(ftype);
            }
        }
        break;

        case StackElement::InterfaceTypeEntry: {
            ObjectTypeEntry *otype = new ObjectTypeEntry(name, since);
            QString targetLangName = attributes["target-lang-name"];
            if (targetLangName.isEmpty())
                targetLangName = name;
            InterfaceTypeEntry *itype =
                new InterfaceTypeEntry(InterfaceTypeEntry::interfaceName(targetLangName), since);

            if (!convertBoolean(attributes["generate"], "generate", true))
                itype->setCodeGeneration(TypeEntry::GenerateForSubclass);
            else
                itype->setCodeGeneration(m_generate);
            otype->setDesignatedInterface(itype);
            itype->setOrigin(otype);
            element->entry = otype;
        }
        // fall through
        case StackElement::NamespaceTypeEntry:
            if (!element->entry)
                element->entry = new NamespaceTypeEntry(name, since);

            // fall through
        case StackElement::ObjectTypeEntry:
            if (!element->entry)
                element->entry = new ObjectTypeEntry(name, since);

            // fall through
        case StackElement::ValueTypeEntry: {
            if (!element->entry)
                element->entry = new ValueTypeEntry(name, since);

            element->entry->setStream(attributes["stream"] == QString("yes"));

            ComplexTypeEntry *ctype = static_cast<ComplexTypeEntry *>(element->entry);
            ctype->setTargetLangPackage(attributes["package"]);
            ctype->setDefaultSuperclass(attributes["default-superclass"]);
            ctype->setGenericClass(convertBoolean(attributes["generic-class"], "generic-class", false));

            if (!convertBoolean(attributes["generate"], "generate", true))
                element->entry->setCodeGeneration(TypeEntry::GenerateForSubclass);
            else
                element->entry->setCodeGeneration(m_generate);

            QString targetLangName = attributes["target-lang-name"];
            if (!targetLangName.isEmpty())
                ctype->setTargetLangName(targetLangName);

            // The expense policy
            QString limit = attributes["expense-limit"];
            if (!limit.isEmpty() && limit != "none") {
                ExpensePolicy ep;
                ep.limit = limit.toInt();
                ep.cost = attributes["expense-cost"];
                ctype->setExpensePolicy(ep);
            }


            ctype->setIsPolymorphicBase(convertBoolean(attributes["polymorphic-base"], "polymorphic-base", false));
            ctype->setPolymorphicIdValue(attributes["polymorphic-id-expression"]);
            //Copyable
            if (attributes["copyable"].isEmpty())
                ctype->setCopyable(ComplexTypeEntry::Unknown);
             else {
                if (convertBoolean(attributes["copyable"], "copyable", false))
                    ctype->setCopyable(ComplexTypeEntry::CopyableSet);
                 else
                    ctype->setCopyable(ComplexTypeEntry::NonCopyableSet);

            }

            if (element->type == StackElement::ObjectTypeEntry || element->type == StackElement::ValueTypeEntry)
                ctype->setHashFunction(attributes["hash-function"]);


            ctype->setHeldType(attributes["held-type"]);

            if (element->type == StackElement::ObjectTypeEntry
                || element->type == StackElement::ValueTypeEntry) {
                if (convertBoolean(attributes["force-abstract"], "force-abstract", false))
                    ctype->setTypeFlags(ctype->typeFlags() | ComplexTypeEntry::ForceAbstract);
                if (convertBoolean(attributes["deprecated"], "deprecated", false))
                    ctype->setTypeFlags(ctype->typeFlags() | ComplexTypeEntry::Deprecated);
            }

            if (element->type == StackElement::InterfaceTypeEntry
                || element->type == StackElement::ValueTypeEntry
                || element->type == StackElement::ObjectTypeEntry) {
                if (convertBoolean(attributes["delete-in-main-thread"], "delete-in-main-thread", false))
                    ctype->setTypeFlags(ctype->typeFlags() | ComplexTypeEntry::DeleteInMainThread);
            }

            QString targetType = attributes["target-type"];
            if (!targetType.isEmpty() && element->entry->isComplex())
                static_cast<ComplexTypeEntry *>(element->entry)->setTargetType(targetType);

            // ctype->setInclude(Include(Include::IncludePath, ctype->name()));
            ctype = ctype->designatedInterface();
            if (ctype)
                ctype->setTargetLangPackage(attributes["package"]);

        }
        break;
        case StackElement::FunctionTypeEntry: {
            QString signature = attributes["signature"];
            signature = TypeDatabase::normalizedSignature(signature.toLatin1().constData());
            element->entry = m_database->findType(name);
            if (element->entry) {
                if (element->entry->type() == TypeEntry::FunctionType) {
                    reinterpret_cast<FunctionTypeEntry*>(element->entry)->addSignature(signature);
                } else {
                    m_error = QString("%1 expected to be a function, but isn't! Maybe it was already declared as a class or something else.").arg(name);
                    return false;
                }
            } else {
                element->entry = new FunctionTypeEntry(name, signature, since);
                element->entry->setCodeGeneration(m_generate);
            }
        }
        break;
        default:
            Q_ASSERT(false);
        };

        if (element->entry)
            m_database->addType(element->entry);
        else
            ReportHandler::warning(QString("Type: %1 was rejected by typesystem").arg(name));

    } else if (element->type == StackElement::InjectDocumentation) {
        // check the XML tag attributes
        QHash<QString, QString> attributes;
        attributes["mode"] = "replace";
        attributes["format"] = "native";
        attributes["since"] = QString("0");

        fetchAttributeValues(tagName, atts, &attributes);
        double since = attributes["since"].toDouble();

        const int validParent = StackElement::TypeEntryMask
                                | StackElement::ModifyFunction
                                | StackElement::ModifyField;
        if (m_current->parent && m_current->parent->type & validParent) {
            QString modeName = attributes["mode"];
            DocModification::Mode mode;
            if (modeName == "append") {
                mode = DocModification::Append;
            } else if (modeName == "prepend") {
                mode = DocModification::Prepend;
            } else if (modeName == "replace") {
                mode = DocModification::Replace;
            } else {
                m_error = "Unknow documentation injection mode: " + modeName;
                return false;
            }

            static QHash<QString, TypeSystem::Language> languageNames;
            if (languageNames.isEmpty()) {
                languageNames["target"] = TypeSystem::TargetLangCode;
                languageNames["native"] = TypeSystem::NativeCode;
            }

            QString format = attributes["format"].toLower();
            TypeSystem::Language lang = languageNames.value(format, TypeSystem::NoLanguage);
            if (lang == TypeSystem::NoLanguage) {
                m_error = QString("unsupported class attribute: '%1'").arg(format);
                return false;
            }

            QString signature = m_current->type & StackElement::TypeEntryMask ? QString() : m_currentSignature;
            DocModification mod(mode, signature, since);
            mod.format = lang;
            m_docModifications << mod;
        } else {
            m_error = "inject-documentation must be inside modify-function, "
                      "modify-field or other tags that creates a type";
            return false;
        }
    } else if (element->type == StackElement::ModifyDocumentation) {
        // check the XML tag attributes
        QHash<QString, QString> attributes;
        attributes["xpath"] = QString();
        attributes["since"] = QString("0");
        fetchAttributeValues(tagName, atts, &attributes);
        double since = attributes["since"].toDouble();

        const int validParent = StackElement::TypeEntryMask
                                | StackElement::ModifyFunction
                                | StackElement::ModifyField;
        if (m_current->parent && m_current->parent->type & validParent) {
            QString signature = (m_current->type & StackElement::TypeEntryMask) ? QString() : m_currentSignature;
            m_docModifications << DocModification(attributes["xpath"], signature, since);
        } else {
            m_error = "modify-documentation must be inside modify-function, "
                      "modify-field or other tags that creates a type";
            return false;
        }
    } else if (element->type != StackElement::None) {
        bool topLevel = element->type == StackElement::Root
                        || element->type == StackElement::SuppressedWarning
                        || element->type == StackElement::Rejection
                        || element->type == StackElement::LoadTypesystem
                        || element->type == StackElement::InjectCode
                        || element->type == StackElement::ConversionRule
                        || element->type == StackElement::AddFunction
                        || element->type == StackElement::Template;

        if (!topLevel && m_current->type == StackElement::Root) {
            m_error = QString("Tag requires parent: '%1'").arg(tagName);
            return false;
        }

        StackElement topElement = !m_current ? StackElement(0) : *m_current;
        element->entry = topElement.entry;

        QHash<QString, QString> attributes;
        attributes["since"] = QString("0");
        switch (element->type) {
        case StackElement::Root:
            attributes["package"] = QString();
            attributes["default-superclass"] = QString();
            break;
        case StackElement::LoadTypesystem:
            attributes["name"] = QString();
            attributes["generate"] = "yes";
            break;
        case StackElement::NoNullPointers:
            attributes["default-value"] = QString();
            break;
        case StackElement::SuppressedWarning:
            attributes["text"] = QString();
            break;
        case StackElement::ReplaceDefaultExpression:
            attributes["with"] = QString();
            break;
        case StackElement::DefineOwnership:
            attributes["class"] = "target";
            attributes["owner"] = "";
            break;
        case StackElement::AddFunction:
            attributes["signature"] = QString();
            attributes["return-type"] = QString("void");
            attributes["access"] = QString("public");
            attributes["static"] = QString("no");
            break;
        case StackElement::ModifyFunction:
            attributes["signature"] = QString();
            attributes["access"] = QString();
            attributes["remove"] = QString();
            attributes["rename"] = QString();
            attributes["deprecated"] = QString("no");
            attributes["associated-to"] = QString();
            attributes["virtual-slot"] = QString("no");
            attributes["thread"] = QString("no");
            attributes["allow-thread"] = QString("no");
            break;
        case StackElement::ModifyArgument:
            attributes["index"] = QString();
            attributes["replace-value"] = QString();
            attributes["invalidate-after-use"] = QString("no");
            break;
        case StackElement::ModifyField:
            attributes["name"] = QString();
            attributes["write"] = "true";
            attributes["read"] = "true";
            break;
        case StackElement::Access:
            attributes["modifier"] = QString();
            break;
        case StackElement::Include:
            attributes["file-name"] = QString();
            attributes["location"] = QString();
            break;
        case StackElement::CustomMetaConstructor:
            attributes["name"] = topElement.entry->name().toLower() + "_create";
            attributes["param-name"] = "copy";
            break;
        case StackElement::CustomMetaDestructor:
            attributes["name"] = topElement.entry->name().toLower() + "_delete";
            attributes["param-name"] = "copy";
            break;
        case StackElement::ReplaceType:
            attributes["modified-type"] = QString();
            break;
        case StackElement::InjectCode:
            attributes["class"] = "target";
            attributes["position"] = "beginning";
            attributes["file"] = QString();
            break;
        case StackElement::ConversionRule:
            attributes["class"] = QString();
            attributes["file"] = QString();
            break;
        case StackElement::RejectEnumValue:
            attributes["name"] = "";
            break;
        case StackElement::ArgumentMap:
            attributes["index"] = "1";
            attributes["meta-name"] = QString();
            break;
        case StackElement::Rename:
            attributes["to"] = QString();
            break;
        case StackElement::Rejection:
            attributes["class"] = "*";
            attributes["function-name"] = "*";
            attributes["field-name"] = "*";
            attributes["enum-name"] = "*";
            break;
        case StackElement::Removal:
            attributes["class"] = "all";
            break;
        case StackElement::Template:
            attributes["name"] = QString();
            break;
        case StackElement::TemplateInstanceEnum:
            attributes["name"] = QString();
            break;
        case StackElement::Replace:
            attributes["from"] = QString();
            attributes["to"] = QString();
            break;
        case StackElement::ReferenceCount:
            attributes["action"] = QString();
            attributes["variable-name"] = QString();
            break;
        case StackElement::ParentOwner:
            attributes["index"] = QString();
            attributes["action"] = QString();
        default:
            { };
        };

        double since = 0;
        if (attributes.count() > 0) {
            fetchAttributeValues(tagName, atts, &attributes);
            since = attributes["since"].toDouble();
        }

        switch (element->type) {
        case StackElement::Root:
            m_defaultPackage = attributes["package"];
            m_defaultSuperclass = attributes["default-superclass"];
            element->type = StackElement::Root;
            {
                TypeSystemTypeEntry* moduleEntry = reinterpret_cast<TypeSystemTypeEntry*>(
                        m_database->findType(m_defaultPackage));
                element->entry = moduleEntry ? moduleEntry : new TypeSystemTypeEntry(m_defaultPackage, since);
            }

            if ((m_generate == TypeEntry::GenerateForSubclass ||
                 m_generate == TypeEntry::GenerateNothing) && m_defaultPackage != "")
                TypeDatabase::instance()->addRequiredTargetImport(m_defaultPackage);

            if (!element->entry->qualifiedCppName().isEmpty())
                m_database->addType(element->entry);
            break;
        case StackElement::LoadTypesystem: {
            QString name = attributes["name"];
            if (name.isEmpty()) {
                m_error = "No typesystem name specified";
                return false;
            }
            bool generateChild = (convertBoolean(attributes["generate"], "generate", true) && (m_generate == TypeEntry::GenerateAll));
            if (!m_database->parseFile(name, generateChild)) {
                m_error = QString("Failed to parse: '%1'").arg(name);
                return false;
            }
        }
        break;
        case StackElement::RejectEnumValue: {
            if (!m_currentEnum) {
                m_error = "<reject-enum-value> node must be used inside a <enum-type> node";
                return false;
            }
            QString name = attributes["name"];

            bool added = false;
            if (!name.isEmpty() && m_database->supportedApiVersion(since)) {
                added = true;
                m_currentEnum->addEnumValueRejection(name);
            }

        } break;
        case StackElement::ReplaceType: {
            if (topElement.type != StackElement::ModifyArgument) {
                m_error = "Type replacement can only be specified for argument modifications";
                return false;
            }

            if (attributes["modified-type"].isEmpty()) {
                m_error = "Type replacement requires 'modified-type' attribute";
                return false;
            }

            m_functionMods.last().argument_mods.last().modified_type = attributes["modified-type"];
        }
        break;
        case StackElement::ConversionRule: {
            if (topElement.type != StackElement::ModifyArgument
                && topElement.type != StackElement::ValueTypeEntry
                && topElement.type != StackElement::ObjectTypeEntry
                && topElement.type != StackElement::PrimitiveTypeEntry
                && topElement.type != StackElement::ContainerTypeEntry) {
                m_error = "Conversion rules can only be specified for argument modification, "
                          "value-type, object-type, primitive-type or container-type conversion.";
                return false;
            }

            if (topElement.type == StackElement::ModifyArgument) {
                static QHash<QString, TypeSystem::Language> languageNames;
                if (languageNames.isEmpty()) {
                    languageNames["target"] = TypeSystem::TargetLangCode;
                    languageNames["native"] = TypeSystem::NativeCode;
                }

                QString languageAttribute = attributes["class"].toLower();
                TypeSystem::Language lang = languageNames.value(languageAttribute, TypeSystem::NoLanguage);
                if (lang == TypeSystem::NoLanguage) {
                    m_error = QString("unsupported class attribute: '%1'").arg(lang);
                    return false;
                }

                CodeSnip snip(since);
                snip.language = lang;
                m_functionMods.last().argument_mods.last().conversion_rules.append(snip);
            } else {
                if (topElement.entry->hasConversionRule()) {
                    m_error = "Types can have only one conversion rule";
                    return false;
                }

                QString sourceFile = attributes["file"];
                if (sourceFile.isEmpty()) {
                    m_error = QString("'file' attribute required; the source file containing the"
                                      " containing the conversion functions must be provided");
                    return false;
                }

                //Handler constructor....
                if (m_generate != TypeEntry::GenerateForSubclass
                    && m_generate != TypeEntry::GenerateNothing) {
                    QFile conversionSource(sourceFile);
                    if (conversionSource.open(QIODevice::ReadOnly | QIODevice::Text)) {
                        topElement.entry->setConversionRule(QString::fromUtf8(conversionSource.readAll()));
                    } else {
                        ReportHandler::warning("File containing conversion code for "
                                               + topElement.entry->name()
                                               + " type does not exist or is not redable: "
                                               + sourceFile);
                    }
                }

            }
        }

        break;
        case StackElement::ModifyArgument: {
            if (topElement.type != StackElement::ModifyFunction
                && topElement.type != StackElement::AddFunction) {
                m_error = QString::fromLatin1("argument modification requires function"
                                              " modification as parent, was %1")
                          .arg(topElement.type, 0, 16);
                return false;
            }

            QString index = attributes["index"];
            if (index == "return")
                index = "0";
            else if (index == "this")
                index = "-1";

            bool ok = false;
            int idx = index.toInt(&ok);
            if (!ok) {
                m_error = QString("Cannot convert '%1' to integer").arg(index);
                return false;
            }

            QString replace_value = attributes["replace-value"];

            if (!replace_value.isEmpty() && idx) {
                m_error = QString("replace-value is only supported for return values (index=0).");
                return false;
            }

            ArgumentModification argumentModification = ArgumentModification(idx, since);
            argumentModification.replace_value = replace_value;
            argumentModification.resetAfterUse = convertBoolean(attributes["invalidate-after-use"], "invalidate-after-use", false);
            m_functionMods.last().argument_mods.append(argumentModification);
        }
        break;
        case StackElement::NoNullPointers: {
            if (topElement.type != StackElement::ModifyArgument) {
                m_error = "no-null-pointer requires argument modification as parent";
                return false;
            }

            m_functionMods.last().argument_mods.last().noNullPointers = true;
            if (!m_functionMods.last().argument_mods.last().index)
                m_functionMods.last().argument_mods.last().nullPointerDefaultValue = attributes["default-value"];
             else if (!attributes["default-value"].isEmpty())
                ReportHandler::warning("default values for null pointer guards are only effective for return values");

        }
        break;
        case StackElement::DefineOwnership: {
            if (topElement.type != StackElement::ModifyArgument) {
                m_error = "define-ownership requires argument modification as parent";
                return false;
            }

            static QHash<QString, TypeSystem::Language> languageNames;
            if (languageNames.isEmpty()) {
                languageNames["target"] = TypeSystem::TargetLangCode;
                languageNames["shell"] = TypeSystem::ShellCode;
            }

            QString classAttribute = attributes["class"].toLower();
            TypeSystem::Language lang = languageNames.value(classAttribute, TypeSystem::NoLanguage);
            if (lang == TypeSystem::NoLanguage) {
                m_error = QString("unsupported class attribute: '%1'").arg(classAttribute);
                return false;
            }

            static QHash<QString, TypeSystem::Ownership> ownershipNames;
            if (ownershipNames.isEmpty()) {
                ownershipNames["target"] = TypeSystem::TargetLangOwnership;
                ownershipNames["c++"] = TypeSystem::CppOwnership;
                ownershipNames["default"] = TypeSystem::DefaultOwnership;
            }

            QString ownershipAttribute = attributes["owner"].toLower();
            TypeSystem::Ownership owner = ownershipNames.value(ownershipAttribute, TypeSystem::InvalidOwnership);
            if (owner == TypeSystem::InvalidOwnership) {
                m_error = QString("unsupported owner attribute: '%1'").arg(ownershipAttribute);
                return false;
            }

            m_functionMods.last().argument_mods.last().ownerships[lang] = owner;
        }
        break;
        case StackElement::SuppressedWarning:
            if (attributes["text"].isEmpty())
                ReportHandler::warning("Suppressed warning with no text specified");
            else
                m_database->addSuppressedWarning(attributes["text"]);
            break;
        case StackElement::ArgumentMap: {
            if (!(topElement.type & StackElement::CodeSnipMask)) {
                m_error = "Argument maps requires code injection as parent";
                return false;
            }

            bool ok;
            int pos = attributes["index"].toInt(&ok);
            if (!ok) {
                m_error = QString("Can't convert position '%1' to integer")
                          .arg(attributes["position"]);
                return false;
            }

            if (pos <= 0) {
                m_error = QString("Argument position %1 must be a positive number").arg(pos);
                return false;
            }

            QString meta_name = attributes["meta-name"];
            if (meta_name.isEmpty())
                ReportHandler::warning("Empty meta name in argument map");


            if (topElement.type == StackElement::InjectCodeInFunction)
                m_functionMods.last().snips.last().argumentMap[pos] = meta_name;
             else {
                ReportHandler::warning("Argument maps are only useful for injection of code "
                                       "into functions.");
            }
        }
        break;
        case StackElement::Removal: {
            if (topElement.type != StackElement::ModifyFunction) {
                m_error = "Function modification parent required";
                return false;
            }

            static QHash<QString, TypeSystem::Language> languageNames;
            if (languageNames.isEmpty()) {
                languageNames["target"] = TypeSystem::TargetLangAndNativeCode;
                languageNames["all"] = TypeSystem::All;
            }

            QString languageAttribute = attributes["class"].toLower();
            TypeSystem::Language lang = languageNames.value(languageAttribute, TypeSystem::NoLanguage);
            if (lang == TypeSystem::NoLanguage) {
                m_error = QString("unsupported class attribute: '%1'").arg(languageAttribute);
                return false;
            }

            m_functionMods.last().removal = lang;
        }
        break;
        case StackElement::Rename:
        case StackElement::Access: {
            if (topElement.type != StackElement::ModifyField
                && topElement.type != StackElement::ModifyFunction
                && topElement.type != StackElement::ModifyArgument) {
                m_error = "Function, field  or argument modification parent required";
                return false;
            }

            Modification *mod = 0;
            if (topElement.type == StackElement::ModifyFunction)
                mod = &m_functionMods.last();
            else if (topElement.type == StackElement::ModifyField)
                mod = &m_fieldMods.last();

            QString modifier;
            if (element->type == StackElement::Rename) {
                modifier = "rename";
                QString renamed_to = attributes["to"];
                if (renamed_to.isEmpty()) {
                    m_error = "Rename modifier requires 'to' attribute";
                    return false;
                }

                if (topElement.type == StackElement::ModifyFunction)
                    mod->setRenamedTo(renamed_to);
                else if (topElement.type == StackElement::ModifyField)
                    mod->setRenamedTo(renamed_to);
                else
                    m_functionMods.last().argument_mods.last().renamed_to = renamed_to;
            } else
                modifier = attributes["modifier"].toLower();


            if (modifier.isEmpty()) {
                m_error = "No access modification specified";
                return false;
            }

            static QHash<QString, FunctionModification::Modifiers> modifierNames;
            if (modifierNames.isEmpty()) {
                modifierNames["private"] = Modification::Private;
                modifierNames["public"] = Modification::Public;
                modifierNames["protected"] = Modification::Protected;
                modifierNames["friendly"] = Modification::Friendly;
                modifierNames["rename"] = Modification::Rename;
                modifierNames["final"] = Modification::Final;
                modifierNames["non-final"] = Modification::NonFinal;
            }

            if (!modifierNames.contains(modifier)) {
                m_error = QString("Unknown access modifier: '%1'").arg(modifier);
                return false;
            }

            if (mod)
                mod->modifiers |= modifierNames[modifier];
        }
        break;
        case StackElement::RemoveArgument:
            if (topElement.type != StackElement::ModifyArgument) {
                m_error = "Removing argument requires argument modification as parent";
                return false;
            }

            m_functionMods.last().argument_mods.last().removed = true;
            break;

        case StackElement::ModifyField: {
            QString name = attributes["name"];
            if (name.isEmpty())
                break;
            FieldModification fm;
            fm.name = name;
            fm.modifiers = 0;

            QString read = attributes["read"];
            QString write = attributes["write"];

            if (read == "true") fm.modifiers |= FieldModification::Readable;
            if (write == "true") fm.modifiers |= FieldModification::Writable;

            m_fieldMods << fm;
        }
        break;
        case StackElement::AddFunction: {
            if (!(topElement.type & (StackElement::ComplexTypeEntryMask | StackElement::Root))) {
                m_error = QString::fromLatin1("Add function requires a complex type or a root tag as parent"
                                              ", was=%1").arg(topElement.type, 0, 16);
                return false;
            }
            QString signature = attributes["signature"];

            signature = TypeDatabase::normalizedSignature(signature.toLocal8Bit().constData());
            if (signature.isEmpty()) {
                m_error = "No signature for the added function";
                return false;
            }

            AddedFunction func(signature, attributes["return-type"], since);
            func.setStatic(attributes["static"] == "yes");
            if (!signature.contains("("))
                signature += "()";
            m_currentSignature = signature;

            QString access = attributes["access"].toLower();
            if (!access.isEmpty()) {
                if (access == QLatin1String("protected")) {
                    func.setAccess(AddedFunction::Protected);
                } else if (access == QLatin1String("public")) {
                    func.setAccess(AddedFunction::Public);
                } else {
                    m_error = QString::fromLatin1("Bad access type '%1'").arg(access);
                    return false;
                }
            }

            m_addedFunctions << func;

            FunctionModification mod(since);
            mod.signature = m_currentSignature;
            m_functionMods << mod;
        }
        break;
        case StackElement::ModifyFunction: {
            if (!(topElement.type & StackElement::ComplexTypeEntryMask)) {
                m_error = QString::fromLatin1("Modify function requires complex type as parent"
                                              ", was=%1").arg(topElement.type, 0, 16);
                return false;
            }
            QString signature = attributes["signature"];

            signature = TypeDatabase::normalizedSignature(signature.toLocal8Bit().constData());
            if (signature.isEmpty()) {
                m_error = "No signature for modified function";
                return false;
            }

            FunctionModification mod(since);
            m_currentSignature = mod.signature = signature;

            QString access = attributes["access"].toLower();
            if (!access.isEmpty()) {
                if (access == QLatin1String("private"))
                    mod.modifiers |= Modification::Private;
                else if (access == QLatin1String("protected"))
                    mod.modifiers |= Modification::Protected;
                else if (access == QLatin1String("public"))
                    mod.modifiers |= Modification::Public;
                else if (access == QLatin1String("final"))
                    mod.modifiers |= Modification::Final;
                else if (access == QLatin1String("non-final"))
                    mod.modifiers |= Modification::NonFinal;
                else {
                    m_error = QString::fromLatin1("Bad access type '%1'").arg(access);
                    return false;
                }
            }

            if (convertBoolean(attributes["deprecated"], "deprecated", false))
                mod.modifiers |= Modification::Deprecated;


            QString remove = attributes["remove"].toLower();
            if (!remove.isEmpty()) {
                if (remove == QLatin1String("all"))
                    mod.removal = TypeSystem::All;
                else if (remove == QLatin1String("target"))
                    mod.removal = TypeSystem::TargetLangAndNativeCode;
                else {
                    m_error = QString::fromLatin1("Bad removal type '%1'").arg(remove);
                    return false;
                }
            }

            QString rename = attributes["rename"];
            if (!rename.isEmpty()) {
                mod.renamedToName = rename;
                mod.modifiers |= Modification::Rename;
            }

            QString association = attributes["associated-to"];
            if (!association.isEmpty())
                mod.association = association;

            mod.setIsThread(convertBoolean(attributes["thread"], "thread", false));
            mod.setAllowThread(convertBoolean(attributes["allow-thread"], "allow-thread", false));

            mod.modifiers |= (convertBoolean(attributes["virtual-slot"], "virtual-slot", false) ? Modification::VirtualSlot : 0);

            m_functionMods << mod;
        }
        break;
        case StackElement::ReplaceDefaultExpression:
            if (!(topElement.type & StackElement::ModifyArgument)) {
                m_error = "Replace default expression only allowed as child of argument modification";
                return false;
            }

            if (attributes["with"].isEmpty()) {
                m_error = "Default expression replaced with empty string. Use remove-default-expression instead.";
                return false;
            }

            m_functionMods.last().argument_mods.last().replacedDefaultExpression = attributes["with"];
            break;
        case StackElement::RemoveDefaultExpression:
            m_functionMods.last().argument_mods.last().removedDefaultExpression = true;
            break;
        case StackElement::CustomMetaConstructor:
        case StackElement::CustomMetaDestructor: {
            CustomFunction *func = new CustomFunction(attributes["name"]);
            func->paramName = attributes["param-name"];
            element->value.customFunction = func;
        }
        break;
        case StackElement::ReferenceCount: {
            if (topElement.type != StackElement::ModifyArgument) {
                m_error = "reference-count must be child of modify-argument";
                return false;
            }

            ReferenceCount rc;

            static QHash<QString, ReferenceCount::Action> actions;
            if (actions.isEmpty()) {
                actions["add"] = ReferenceCount::Add;
                //actions["add-all"] = ReferenceCount::AddAll;
                actions["remove"] = ReferenceCount::Remove;
                //actions["set"] = ReferenceCount::Set;
                actions["ignore"] = ReferenceCount::Ignore;
            }
            rc.action = actions.value(attributes["action"].toLower(), ReferenceCount::Invalid);
            rc.varName = attributes["variable-name"];

            if (rc.action == ReferenceCount::Invalid) {
                m_error = "unrecognized value for action attribute. supported actions:";
                foreach (QString action, actions.keys())
                    m_error += " " + action;
            }

            m_functionMods.last().argument_mods.last().referenceCounts.append(rc);
        }
        break;

        case StackElement::ParentOwner: {
            if (topElement.type != StackElement::ModifyArgument) {
                m_error = "parent-policy must be child of modify-argument";
                return false;
            }

            ArgumentOwner ao;

            QString index = attributes["index"];
            if (index == "return")
                index = "0";
            else if (index == "this")
                index = "-1";

            bool ok = false;
            int idx = index.toInt(&ok);
            if (!ok) {
                m_error = QString("Cannot convert '%1' to integer").arg(index);
                return false;
            }

            static QHash<QString, ArgumentOwner::Action> actions;
            if (actions.isEmpty()) {
                actions["add"] = ArgumentOwner::Add;
                actions["remove"] = ArgumentOwner::Remove;
            }

            ao.action = actions.value(attributes["action"].toLower(), ArgumentOwner::Invalid);
            if (!ao.action) {
                m_error = QString("Invalid parent actionr");
                return false;
            }
            ao.index = idx;
            m_functionMods.last().argument_mods.last().owner = ao;
        }
        break;


        case StackElement::InjectCode: {
            if (!(topElement.type & StackElement::ComplexTypeEntryMask)
                && (topElement.type != StackElement::AddFunction)
                && (topElement.type != StackElement::ModifyFunction)
                && (topElement.type != StackElement::Root)) {
                m_error = "wrong parent type for code injection";
                return false;
            }

            static QHash<QString, TypeSystem::Language> languageNames;
            if (languageNames.isEmpty()) {
                languageNames["target"] = TypeSystem::TargetLangCode; // em algum lugar do cpp
                languageNames["native"] = TypeSystem::NativeCode; // em algum lugar do cpp
                languageNames["shell"] = TypeSystem::ShellCode; // coloca no header, mas antes da declaracao da classe
                languageNames["shell-declaration"] = TypeSystem::ShellDeclaration; // coloca no header, dentro da declaracao da classe
                languageNames["library-initializer"] = TypeSystem::PackageInitializer;
                languageNames["destructor-function"] = TypeSystem::DestructorFunction;
                languageNames["constructors"] = TypeSystem::Constructors;
                languageNames["interface"] = TypeSystem::Interface;
            }

            QString className = attributes["class"].toLower();
            if (!languageNames.contains(className)) {
                m_error = QString("Invalid class specifier: '%1'").arg(className);
                return false;
            }


            static QHash<QString, CodeSnip::Position> positionNames;
            if (positionNames.isEmpty()) {
                positionNames["beginning"] = CodeSnip::Beginning;
                positionNames["end"] = CodeSnip::End;
                // QtScript
                positionNames["declaration"] = CodeSnip::Declaration;
                positionNames["prototype-initialization"] = CodeSnip::PrototypeInitialization;
                positionNames["constructor-initialization"] = CodeSnip::ConstructorInitialization;
                positionNames["constructor"] = CodeSnip::Constructor;
            }

            QString position = attributes["position"].toLower();
            if (!positionNames.contains(position)) {
                m_error = QString("Invalid position: '%1'").arg(position);
                return false;
            }

            CodeSnip snip(since);
            snip.language = languageNames[className];
            snip.position = positionNames[position];
            bool in_file = false;

            QString file_name = attributes["file"];

            //Handler constructor....
            if (m_generate != TypeEntry::GenerateForSubclass &&
                m_generate != TypeEntry::GenerateNothing &&
                !file_name.isEmpty()) {
                if (QFile::exists(file_name)) {
                    QFile codeFile(file_name);
                    if (codeFile.open(QIODevice::Text | QIODevice::ReadOnly)) {
                        QString content = QString::fromUtf8(codeFile.readAll());
                        content.prepend("// ========================================================================\n"
                                        "// START of custom code block [file: " + file_name + "]\n");
                        content.append("\n// END of custom code block [file: " + file_name + "]\n"
                                       "// ========================================================================\n");
                        snip.addCode(content);
                        in_file = true;
                    }
                } else
                    ReportHandler::warning("File for inject code not exist: " + file_name);

            }

            if (snip.language == TypeSystem::Interface && topElement.type != StackElement::InterfaceTypeEntry) {
                m_error = "Interface code injections must be direct child of an interface type entry";
                return false;
            }

            if (topElement.type == StackElement::ModifyFunction || topElement.type == StackElement::AddFunction) {
                FunctionModification mod = m_functionMods.last();
                if (snip.language == TypeSystem::ShellDeclaration) {
                    m_error = "no function implementation in shell declaration in which to inject code";
                    return false;
                }

                m_functionMods.last().snips << snip;
                if (in_file)
                    m_functionMods.last().modifiers |= FunctionModification::CodeInjection;
                element->type = StackElement::InjectCodeInFunction;
            } else if (topElement.type == StackElement::Root) {
                element->entry->addCodeSnip(snip);
            } else if (topElement.type != StackElement::Root) {
                m_codeSnips << snip;
            }

        }
        break;
        case StackElement::Include: {
            QString location = attributes["location"].toLower();

            static QHash<QString, Include::IncludeType> locationNames;
            if (locationNames.isEmpty()) {
                locationNames["global"] = Include::IncludePath;
                locationNames["local"] = Include::LocalPath;
                locationNames["target"] = Include::TargetLangImport;
            }

            if (!locationNames.contains(location)) {
                m_error = QString("Location not recognized: '%1'").arg(location);
                return false;
            }

            Include::IncludeType loc = locationNames[location];
            Include inc(loc, attributes["file-name"]);

            ComplexTypeEntry *ctype = static_cast<ComplexTypeEntry *>(element->entry);
            if (topElement.type & (StackElement::ComplexTypeEntryMask | StackElement::PrimitiveTypeEntry)) {
                element->entry->setInclude(inc);
            } else if (topElement.type == StackElement::ExtraIncludes) {
                element->entry->addExtraInclude(inc);
            } else {
                m_error = "Only supported parent tags are primitive-type, complex types or extra-includes";
                return false;
            }

            inc = ctype->include();
            IncludeList lst = ctype->extraIncludes();
            ctype = ctype->designatedInterface();
            if (ctype) {
                ctype->setExtraIncludes(lst);
                ctype->setInclude(inc);
            }
        }
        break;
        case StackElement::Rejection: {
            QString cls = attributes["class"];
            QString function = attributes["function-name"];
            QString field = attributes["field-name"];
            QString enum_ = attributes["enum-name"];
            if (cls == "*" && function == "*" && field == "*" && enum_ == "*") {
                m_error = "bad reject entry, neither 'class', 'function-name' nor "
                          "'field' specified";
                return false;
            }
            m_database->addRejection(cls, function, field, enum_);
        }
        break;
        case StackElement::Template:
            element->value.templateEntry = new TemplateEntry(attributes["name"], since);
            break;
        case StackElement::TemplateInstanceEnum:
            if (!(topElement.type & StackElement::CodeSnipMask) &&
                (topElement.type != StackElement::Template) &&
                (topElement.type != StackElement::CustomMetaConstructor) &&
                (topElement.type != StackElement::CustomMetaDestructor) &&
                (topElement.type != StackElement::ConversionRule)) {
                m_error = "Can only insert templates into code snippets, templates, custom-constructors, custom-destructors or conversion-rule.";
                return false;
            }
            element->value.templateInstance = new TemplateInstance(attributes["name"], since);
            break;
        case StackElement::Replace:
            if (topElement.type != StackElement::TemplateInstanceEnum) {
                m_error = "Can only insert replace rules into insert-template.";
                return false;
            }
            element->parent->value.templateInstance->addReplaceRule(attributes["from"], attributes["to"]);
            break;
        default:
            break; // nada
        };
    }

    m_current = element;
    return true;
}

PrimitiveTypeEntry* PrimitiveTypeEntry::basicAliasedTypeEntry() const
{
    if (!m_aliasedTypeEntry)
        return 0;

    PrimitiveTypeEntry* baseAliasTypeEntry = m_aliasedTypeEntry->basicAliasedTypeEntry();
    if (baseAliasTypeEntry)
        return baseAliasTypeEntry;
    else
        return m_aliasedTypeEntry;
}

CodeSnipList TypeEntry::codeSnips() const
{
    CodeSnipList lst;
    TypeDatabase *td = TypeDatabase::instance();
    foreach(CodeSnip cs, m_codeSnips) {
        if (td->supportedApiVersion(cs.version))
            lst.append(cs);
    }
    return lst;
}

QString Modification::accessModifierString() const
{
    if (isPrivate()) return "private";
    if (isProtected()) return "protected";
    if (isPublic()) return "public";
    if (isFriendly()) return "friendly";
    return QString();
}

FunctionModificationList ComplexTypeEntry::functionModifications(const QString &signature) const
{
    FunctionModificationList lst;
    TypeDatabase *td = TypeDatabase::instance();
    for (int i = 0; i < m_functionMods.count(); ++i) {
        const FunctionModification &mod = m_functionMods.at(i);
        if ((mod.signature == signature) && (td->supportedApiVersion(mod.version())))
            lst << mod;
    }

    return lst;
}

FieldModification ComplexTypeEntry::fieldModification(const QString &name) const
{
    for (int i = 0; i < m_fieldMods.size(); ++i)
        if (m_fieldMods.at(i).name == name)
            return m_fieldMods.at(i);
    FieldModification mod;
    mod.name = name;
    mod.modifiers = FieldModification::Readable | FieldModification::Writable;
    return mod;
}

QString ContainerTypeEntry::targetLangPackage() const
{
    return QString();
}

QString ContainerTypeEntry::targetLangName() const
{

    switch (m_type) {
    case StringListContainer: return "QStringList";
    case ListContainer: return "QList";
    case LinkedListContainer: return "QLinkedList";
    case VectorContainer: return "QVector";
    case StackContainer: return "QStack";
    case QueueContainer: return "QQueue";
    case SetContainer: return "QSet";
    case MapContainer: return "QMap";
    case MultiMapContainer: return "QMultiMap";
    case HashContainer: return "QHash";
    case MultiHashContainer: return "QMultiHash";
    case PairContainer: return "QPair";
    default:
        qWarning("bad type... %d", m_type);
        break;
    }
    return QString();
}

QString ContainerTypeEntry::qualifiedCppName() const
{
    if (m_type == StringListContainer)
        return "QStringList";
    return ComplexTypeEntry::qualifiedCppName();
}

QString EnumTypeEntry::targetLangQualifier() const
{
    TypeEntry *te = TypeDatabase::instance()->findType(m_qualifier);
    if (te)
        return te->targetLangName();
    else
        return m_qualifier;
}

QString EnumTypeEntry::targetLangApiName() const
{
    return "jint";
}

QString FlagsTypeEntry::targetLangApiName() const
{
    return "jint";
}

void EnumTypeEntry::addEnumValueRedirection(const QString &rejected, const QString &usedValue)
{
    m_enumRedirections << EnumValueRedirection(rejected, usedValue);
}

QString EnumTypeEntry::enumValueRedirection(const QString &value) const
{
    for (int i = 0; i < m_enumRedirections.size(); ++i)
        if (m_enumRedirections.at(i).rejected == value)
            return m_enumRedirections.at(i).used;
    return QString();
}

QString FlagsTypeEntry::qualifiedTargetLangName() const
{
    return targetLangPackage() + "." + m_enum->targetLangQualifier() + "." + targetLangName();
}

/*!
 * The Visual Studio 2002 compiler doesn't support these symbols,
 * which our typedefs unforntuatly expand to.
 */
QString fixCppTypeName(const QString &name)
{
    if (name == "long long") return "qint64";
    else if (name == "unsigned long long") return "quint64";
    return name;
}

QString TemplateInstance::expandCode() const
{
    TemplateEntry *templateEntry = TypeDatabase::instance()->findTemplate(m_name);
    if (templateEntry) {
        QString res = templateEntry->code();
        foreach (QString key, replaceRules.keys())
            res.replace(key, replaceRules[key]);

        return "// TEMPLATE - " + m_name + " - START" + res + "// TEMPLATE - " + m_name + " - END";
    } else
        ReportHandler::warning("insert-template referring to non-existing template '" + m_name + "'");

    return QString();
}


QString CodeSnipAbstract::code() const
{
    QString res;
    foreach (CodeSnipFragment codeFrag, codeList)
        res.append(codeFrag.code());

    return res;
}

QString CodeSnipFragment::code() const
{
    if (m_instance)
        return m_instance->expandCode();
    else
        return m_code;
}

QString FunctionModification::toString() const
{
    QString str = signature + QLatin1String("->");
    if (modifiers & AccessModifierMask) {
        switch (modifiers & AccessModifierMask) {
        case Private: str += QLatin1String("private"); break;
        case Protected: str += QLatin1String("protected"); break;
        case Public: str += QLatin1String("public"); break;
        case Friendly: str += QLatin1String("friendly"); break;
        }
    }

    if (modifiers & Final) str += QLatin1String("final");
    if (modifiers & NonFinal) str += QLatin1String("non-final");

    if (modifiers & Readable) str += QLatin1String("readable");
    if (modifiers & Writable) str += QLatin1String("writable");

    if (modifiers & CodeInjection) {
        foreach (CodeSnip s, snips) {
            str += QLatin1String("\n//code injection:\n");
            str += s.code();
        }
    }

    if (modifiers & Rename) str += QLatin1String("renamed:") + renamedToName;

    if (modifiers & Deprecated) str += QLatin1String("deprecate");

    if (modifiers & ReplaceExpression) str += QLatin1String("replace-expression");

    return str;
}

bool FunctionModification::operator!=(const FunctionModification& other) const
{
    return !(*this == other);
}

bool FunctionModification::operator==(const FunctionModification& other) const
{
    if (signature != other.signature)
        return false;

    if (association != other.association)
        return false;

    if (modifiers != other.modifiers)
        return false;

    if (removal != other.removal)
        return false;

    if (m_thread != other.m_thread)
        return false;

    if (m_allowThread != other.m_allowThread)
        return false;

    if (m_version != other.m_version)
        return false;

    return true;
}

static AddedFunction::TypeInfo parseType(const QString& signature, int startPos = 0, int* endPos = 0)
{
    AddedFunction::TypeInfo result;
    QRegExp regex("\\w");
    int length = signature.length();
    int start = signature.indexOf(regex, startPos);
    if (start == -1) {
        if (signature.mid(startPos + 1, 3) == "...") { // varargs
            if (endPos)
                *endPos = startPos + 4;
            result.name = "...";
        } else { // error
            if (endPos)
                *endPos = length;
        }
        return result;
    }

    int cantStop = 0;
    QString paramString;
    QChar c;
    int i = start;
    for (; i < length; ++i) {
        c = signature[i];
        if (c == '<')
            cantStop++;
        if (c == '>')
            cantStop--;
        if (cantStop < 0)
            break; // FIXME: report error?
        if ((c == ')' || c == ',') && !cantStop)
            break;
        paramString += signature[i];
    }
    if (endPos)
        *endPos = i;

    // Check default value
    if (paramString.contains('=')) {
        QStringList lst = paramString.split('=');
        paramString = lst[0].trimmed();
        result.defaultValue = lst[1].trimmed();
    }

    // check constness
    if (paramString.startsWith("const ")) {
        result.isConstant = true;
        paramString.remove(0, sizeof("const")/sizeof(char));
        paramString = paramString.trimmed();
    }
    // check reference
    if (paramString.endsWith("&")) {
        result.isReference = true;
        paramString.chop(1);
        paramString = paramString.trimmed();
    }
    // check Indirections
    while (paramString.endsWith("*")) {
        result.indirections++;
        paramString.chop(1);
        paramString = paramString.trimmed();
    }
    result.name = paramString;

    return result;
}

AddedFunction::AddedFunction(QString signature, QString returnType, double vr) : m_access(Public), m_version(vr)
{
    Q_ASSERT(!returnType.isEmpty());
    m_returnType = parseType(returnType);
    signature = signature.trimmed();
    int endPos = signature.indexOf('(');
    if (endPos < 0) {
        m_isConst = false;
        m_name = signature;
    } else {
        m_name = signature.left(endPos).trimmed();
        int signatureLength = signature.length();
        while (endPos < signatureLength) {
            TypeInfo arg = parseType(signature, endPos, &endPos);
            if (!arg.name.isEmpty())
                m_arguments.append(arg);
            // end of parameters...
            if (signature[endPos] == ')')
                break;
        }
        // is const?
        m_isConst = signature.right(signatureLength - endPos).contains("const");
    }
}

QString ComplexTypeEntry::targetLangApiName() const
{
    return strings_jobject;
}
QString StringTypeEntry::targetLangApiName() const
{
    return strings_jobject;
}
QString StringTypeEntry::targetLangName() const
{
    return strings_String;
}
QString StringTypeEntry::targetLangPackage() const
{
    return QString();
}
QString CharTypeEntry::targetLangApiName() const
{
    return strings_jchar;
}
QString CharTypeEntry::targetLangName() const
{
    return strings_char;
}
QString VariantTypeEntry::targetLangApiName() const
{
    return strings_jobject;
}
QString VariantTypeEntry::targetLangName() const
{
    return strings_Object;
}
QString VariantTypeEntry::targetLangPackage() const
{
    return QString();
}

QString ContainerTypeEntry::typeName() const
{
    switch(m_type) {
        case LinkedListContainer:
            return "linked-list";
        case ListContainer:
            return "list";
        case StringListContainer:
            return "string-list";
        case VectorContainer:
            return "vector";
        case StackContainer:
            return "stack";
        case QueueContainer:
            return "queue";
        case SetContainer:
            return "set";
        case MapContainer:
            return "map";
        case MultiMapContainer:
            return "multi-map";
        case HashContainer:
            return "hash";
        case MultiHashContainer:
            return "multi-hash";
        case PairContainer:
            return "pair";
        case NoContainer:
        default:
            return "?";
    }
}


/*
static void injectCode(ComplexTypeEntry *e,
                       const char *signature,
                       const QByteArray &code,
                       const ArgumentMap &args)
{
    CodeSnip snip;
    snip.language = TypeSystem::NativeCode;
    snip.position = CodeSnip::Beginning;
    snip.addCode(QString::fromLatin1(code));
    snip.argumentMap = args;

    FunctionModification mod;
    mod.signature = QMetaObject::normalizedSignature(signature);
    mod.snips << snip;
    mod.modifiers = Modification::CodeInjection;
}
*/

