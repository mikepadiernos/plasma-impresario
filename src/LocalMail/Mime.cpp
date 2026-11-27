/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "LocalMail/Mime.h"

namespace LocalMail {
namespace Mime {

namespace {

bool isSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

QByteArray unfold(const QByteArray &value)
{
    QByteArray result;
    result.reserve(value.size());
    for (int i = 0; i < value.size(); ++i) {
        const char c = value.at(i);
        if (c == '\r' || c == '\n')
            continue;
        result.append(c);
    }
    return result.trimmed();
}

/** Remove the quotes and backslashes of a quoted string; other text is kept */
QByteArray unquote(const QByteArray &text)
{
    QByteArray result;
    bool quoted = false;
    for (int i = 0; i < text.size(); ++i) {
        const char c = text.at(i);
        if (c == '\\' && i + 1 < text.size()) {
            result.append(text.at(++i));
        } else if (c == '"') {
            quoted = !quoted;
        } else {
            result.append(c);
        }
    }
    return result;
}

/** Split @arg value at each @arg separator which is outside quotes, comments and angle brackets */
QList<QByteArray> splitOutside(const QByteArray &value, const QByteArray &separators)
{
    QList<QByteArray> result;
    QByteArray current;
    bool quoted = false;
    int comment = 0;
    int angle = 0;
    for (int i = 0; i < value.size(); ++i) {
        const char c = value.at(i);
        if (quoted) {
            current.append(c);
            if (c == '\\' && i + 1 < value.size())
                current.append(value.at(++i));
            else if (c == '"')
                quoted = false;
            continue;
        }
        if (!comment && !angle && separators.contains(c)) {
            result.append(current);
            current.clear();
            continue;
        }
        if (c == '"' && !comment) {
            quoted = true;
        } else if (c == '(') {
            ++comment;
        } else if (c == ')' && comment) {
            --comment;
        } else if (c == '<' && !comment) {
            ++angle;
        } else if (c == '>' && !comment && angle) {
            --angle;
        }
        current.append(c);
    }
    result.append(current);
    return result;
}

/** @arg text without its comments; the last comment's text goes to @arg lastComment */
QByteArray withoutComments(const QByteArray &text, QByteArray *lastComment = nullptr)
{
    QByteArray result;
    QByteArray comment;
    bool quoted = false;
    int depth = 0;
    for (int i = 0; i < text.size(); ++i) {
        const char c = text.at(i);
        if (depth) {
            if (c == '\\' && i + 1 < text.size()) {
                comment.append(text.at(++i));
            } else if (c == '(') {
                ++depth;
                comment.append(c);
            } else if (c == ')') {
                if (--depth == 0) {
                    if (lastComment)
                        *lastComment = comment.trimmed();
                    comment.clear();
                } else {
                    comment.append(c);
                }
            } else {
                comment.append(c);
            }
            continue;
        }
        if (quoted) {
            result.append(c);
            if (c == '\\' && i + 1 < text.size())
                result.append(text.at(++i));
            else if (c == '"')
                quoted = false;
        } else if (c == '"') {
            quoted = true;
            result.append(c);
        } else if (c == '(') {
            depth = 1;
        } else {
            result.append(c);
        }
    }
    return result;
}

QByteArray simplified(const QByteArray &text)
{
    return text.simplified();
}

using Parameters = QList<QPair<QByteArray, QByteArray>>;

/** Read "value; name=value; ..." into the lowercase first value and its parameters */
QByteArray parseWithParameters(const QByteArray &field, Parameters *parameters)
{
    const QList<QByteArray> items = splitOutside(withoutComments(field), ";");
    for (int i = 1; i < items.size(); ++i) {
        const int equals = items[i].indexOf('=');
        if (equals <= 0)
            continue;
        const QByteArray name = items[i].left(equals).trimmed().toLower();
        const QByteArray value = unquote(items[i].mid(equals + 1).trimmed());
        if (!name.isEmpty())
            parameters->append({name, value});
    }
    return items.isEmpty() ? QByteArray() : items.first().trimmed().toLower();
}

QByteArray parameter(const Parameters &parameters, const QByteArray &name)
{
    for (const auto &item : parameters) {
        if (item.first == name)
            return item.second;
    }
    return {};
}

QByteArray quoted(const QByteArray &value)
{
    QByteArray result = "\"";
    for (const char c : value) {
        if (c == '"' || c == '\\')
            result.append('\\');
        result.append(c);
    }
    return result + '"';
}

QByteArray parameterList(const Parameters &parameters)
{
    if (parameters.isEmpty())
        return "NIL";
    QList<QByteArray> items;
    for (const auto &item : parameters)
        items << nstring(item.first.toUpper(), false) << nstring(item.second, false);
    return '(' + items.join(' ') + ')';
}

struct Address {
    QByteArray name;
    QByteArray route;
    QByteArray mailbox;
    QByteArray host;
};

QList<Address> parseAddresses(const QByteArray &value)
{
    QList<Address> result;
    for (QByteArray chunk : splitOutside(value, ",;")) {
        // A group's name is dropped; its members are listed like the others
        const QList<QByteArray> group = splitOutside(chunk, ":");
        if (group.size() > 1 && !group.first().contains('@'))
            chunk = chunk.mid(group.first().size() + 1);
        Address address;
        QByteArray spec;
        const int open = chunk.indexOf('<');
        const QList<QByteArray> beforeAngle = splitOutside(chunk, "<");
        if (open >= 0 && beforeAngle.size() == 1) {
            // "<" inside quotes or comments only
            spec = withoutComments(chunk, &address.name);
        } else if (open >= 0) {
            address.name = simplified(unquote(withoutComments(beforeAngle.first())));
            QByteArray inside = chunk.mid(beforeAngle.first().size() + 1);
            const int close = inside.lastIndexOf('>');
            if (close >= 0)
                inside.truncate(close);
            spec = inside;
        } else {
            QByteArray comment;
            spec = withoutComments(chunk, &comment);
            address.name = simplified(comment);
        }
        spec = spec.trimmed();
        if (spec.startsWith('@')) {
            const int colon = spec.indexOf(':');
            if (colon > 0) {
                address.route = spec.left(colon);
                spec = spec.mid(colon + 1);
            }
        }
        QByteArray compact;
        for (const char c : spec) {
            if (!isSpace(c))
                compact.append(c);
        }
        const int at = compact.lastIndexOf('@');
        if (at >= 0) {
            address.mailbox = compact.left(at);
            address.host = compact.mid(at + 1);
        } else {
            address.mailbox = compact;
        }
        if (address.mailbox.isEmpty() && address.name.isEmpty())
            continue;
        result.append(address);
    }
    return result;
}

QByteArray addressList(const QByteArray &value)
{
    const QList<Address> addresses = parseAddresses(value);
    if (addresses.isEmpty())
        return "NIL";
    QByteArray result = "(";
    for (const Address &address : addresses) {
        result += '(' + nstring(address.name) + ' ' + nstring(address.route) + ' ' + nstring(address.mailbox) + ' '
            + nstring(address.host) + ')';
    }
    return result + ')';
}

int countLines(const QByteArray &body)
{
    int lines = body.count("\r\n");
    if (!body.isEmpty() && !body.endsWith("\r\n"))
        ++lines;
    return lines;
}

Part parsePart(const QByteArray &raw, bool digest);

void splitMultipart(Part &part)
{
    const QByteArray boundary = parameter(part.parameters, "boundary");
    const QByteArray delimiter = "--" + boundary;
    QList<int> starts;
    QList<int> ends;
    int position = 0;
    bool closed = false;
    while (!boundary.isEmpty() && !closed) {
        int found = part.body.indexOf(delimiter, position);
        while (found > 0 && part.body.mid(found - 2, 2) != "\r\n")
            found = part.body.indexOf(delimiter, found + 1);
        if (found < 0)
            break;
        // The line end before a delimiter belongs to it
        if (!starts.isEmpty())
            ends.append(found >= 2 ? found - 2 : found);
        const int after = found + delimiter.size();
        closed = part.body.mid(after, 2) == "--";
        const int lineEnd = part.body.indexOf("\r\n", after);
        if (closed || lineEnd < 0)
            break;
        starts.append(lineEnd + 2);
        position = lineEnd + 2;
    }
    if (ends.size() < starts.size())
        ends.append(part.body.size());
    const bool digest = part.subtype == "digest";
    for (int i = 0; i < starts.size(); ++i)
        part.children.append(parsePart(part.body.mid(starts[i], qMax(0, ends[i] - starts[i])), digest));
    if (part.children.isEmpty()) {
        // Without parts it cannot be described as a multipart
        part.type = "text";
        part.subtype = "plain";
        part.parameters = {{"charset", "us-ascii"}};
    }
}

Part parsePart(const QByteArray &raw, bool digest)
{
    Part part;
    if (raw.startsWith("\r\n")) {
        part.header = Header(raw.left(2));
        part.body = raw.mid(2);
    } else {
        const int end = raw.indexOf("\r\n\r\n");
        if (end < 0) {
            part.header = Header(raw);
        } else {
            part.header = Header(raw.left(end + 4));
            part.body = raw.mid(end + 4);
        }
    }

    const QByteArray contentType = part.header.value("content-type");
    Parameters parameters;
    const QByteArray mediaType = parseWithParameters(contentType, &parameters);
    const int slash = mediaType.indexOf('/');
    if (slash > 0 && slash + 1 < mediaType.size()) {
        part.type = mediaType.left(slash).trimmed();
        part.subtype = mediaType.mid(slash + 1).trimmed();
        part.parameters = parameters;
    } else if (digest) {
        part.type = "message";
        part.subtype = "rfc822";
    } else {
        part.parameters = {{"charset", "us-ascii"}};
    }
    if (part.type == "text" && parameter(part.parameters, "charset").isEmpty())
        part.parameters.prepend({"charset", "us-ascii"});

    const QByteArray disposition = part.header.value("content-disposition");
    if (!disposition.isEmpty())
        part.disposition = parseWithParameters(disposition, &part.dispositionParameters);

    if (part.isMultipart())
        splitMultipart(part);
    else if (part.isMessage())
        part.children.append(parsePart(part.body, false));
    return part;
}

QByteArray structure(const Part &part, bool extended)
{
    const QByteArray language = part.header.value("content-language");
    const QByteArray location = part.header.value("content-location");
    QByteArray disposition = "NIL";
    if (!part.disposition.isEmpty())
        disposition = '(' + quoted(part.disposition.toUpper()) + ' ' + parameterList(part.dispositionParameters) + ')';

    if (part.isMultipart()) {
        QByteArray result = "(";
        for (const Part &child : part.children)
            result += structure(child, extended);
        result += ' ' + quoted(part.subtype.toUpper());
        if (extended)
            result += ' ' + parameterList(part.parameters) + ' ' + disposition + ' ' + nstring(language) + ' '
                + nstring(location);
        return result + ')';
    }

    QByteArray encoding = unfold(part.header.value("content-transfer-encoding")).toUpper();
    if (encoding.isEmpty())
        encoding = "7BIT";
    QByteArray result = '(' + quoted(part.type.toUpper()) + ' ' + quoted(part.subtype.toUpper()) + ' '
        + parameterList(part.parameters) + ' ' + nstring(part.header.value("content-id")) + ' '
        + nstring(part.header.value("content-description")) + ' ' + quoted(encoding) + ' '
        + QByteArray::number(part.body.size());
    if (part.isMessage() && !part.children.isEmpty()) {
        result += ' ' + envelope(part.children.first().header) + ' ' + structure(part.children.first(), extended) + ' '
            + QByteArray::number(countLines(part.body));
    } else if (part.type == "text") {
        result += ' ' + QByteArray::number(countLines(part.body));
    }
    if (extended)
        result += ' ' + nstring(part.header.value("content-md5")) + ' ' + disposition + ' ' + nstring(language) + ' '
            + nstring(location);
    return result + ')';
}

QList<QByteArray> fieldNames(const QByteArray &list)
{
    QByteArray inside = list.trimmed();
    if (inside.startsWith('('))
        inside = inside.mid(1);
    if (inside.endsWith(')'))
        inside.chop(1);
    QList<QByteArray> names;
    for (const QByteArray &name : inside.simplified().split(' ')) {
        if (!name.isEmpty())
            names << unquote(name).toLower();
    }
    return names;
}

}

Header::Header(const QByteArray &raw)
    : m_raw(raw)
{
    int position = 0;
    while (position < raw.size()) {
        int end = raw.indexOf("\r\n", position);
        if (end < 0)
            end = raw.size();
        else
            end += 2;
        const QByteArray line = raw.mid(position, end - position);
        position = end;
        if (line == "\r\n")
            break;
        if ((line.startsWith(' ') || line.startsWith('\t')) && !m_fields.isEmpty()) {
            m_fields.last().original += line;
            continue;
        }
        const int colon = line.indexOf(':');
        if (colon <= 0)
            continue;
        m_fields.append({line.left(colon).trimmed().toLower(), line});
    }
}

QByteArray Header::raw() const
{
    return m_raw;
}

QByteArray Header::value(const QByteArray &name) const
{
    const QByteArray wanted = name.toLower();
    for (const Field &field : m_fields) {
        if (field.name == wanted)
            return unfold(field.original.mid(field.original.indexOf(':') + 1));
    }
    return {};
}

bool Header::contains(const QByteArray &name) const
{
    const QByteArray wanted = name.toLower();
    for (const Field &field : m_fields) {
        if (field.name == wanted)
            return true;
    }
    return false;
}

QByteArray Header::fields(const QList<QByteArray> &names, bool exclude) const
{
    QByteArray result;
    for (const Field &field : m_fields) {
        if (names.contains(field.name) != exclude) {
            result += field.original;
            if (!field.original.endsWith("\r\n"))
                result += "\r\n";
        }
    }
    return result + "\r\n";
}

Part parse(const QByteArray &raw)
{
    return parsePart(raw, false);
}

QByteArray canonicalLineEnds(const QByteArray &raw)
{
    QByteArray result;
    result.reserve(raw.size() + raw.size() / 40);
    for (int i = 0; i < raw.size(); ++i) {
        const char c = raw.at(i);
        if (c == '\r') {
            result += "\r\n";
            if (i + 1 < raw.size() && raw.at(i + 1) == '\n')
                ++i;
        } else if (c == '\n') {
            result += "\r\n";
        } else {
            result += c;
        }
    }
    return result;
}

QByteArray nstring(const QByteArray &value, bool emptyIsNil)
{
    if (value.isEmpty() && emptyIsNil)
        return "NIL";
    bool literal = value.size() > 1000;
    for (int i = 0; !literal && i < value.size(); ++i) {
        const auto c = static_cast<unsigned char>(value.at(i));
        literal = c == 0 || c == '\r' || c == '\n' || c >= 0x80;
    }
    if (literal)
        return '{' + QByteArray::number(value.size()) + "}\r\n" + value;
    return quoted(value);
}

QByteArray envelope(const Header &header)
{
    const QByteArray from = header.value("from");
    const QByteArray sender = header.contains("sender") ? header.value("sender") : from;
    const QByteArray replyTo = header.contains("reply-to") ? header.value("reply-to") : from;
    return '(' + nstring(header.value("date")) + ' ' + nstring(header.value("subject")) + ' ' + addressList(from) + ' '
        + addressList(sender) + ' ' + addressList(replyTo) + ' ' + addressList(header.value("to")) + ' '
        + addressList(header.value("cc")) + ' ' + addressList(header.value("bcc")) + ' '
        + nstring(header.value("in-reply-to")) + ' ' + nstring(header.value("message-id")) + ')';
}

QByteArray bodyStructure(const Part &part, bool extended)
{
    return structure(part, extended);
}

bool section(const Part &message, const QByteArray &raw, const QByteArray &spec, QByteArray *out)
{
    // Part numbers come first, then an optional text part name
    QList<int> numbers;
    QByteArray rest = spec.trimmed();
    while (!rest.isEmpty() && rest.at(0) >= '0' && rest.at(0) <= '9') {
        int end = 0;
        while (end < rest.size() && rest.at(end) >= '0' && rest.at(end) <= '9')
            ++end;
        bool ok = false;
        const int number = rest.left(end).toInt(&ok);
        if (!ok || number < 1)
            return false;
        numbers << number;
        rest = rest.mid(end);
        if (rest.startsWith('.'))
            rest = rest.mid(1);
        else if (!rest.isEmpty())
            return false;
    }
    const QByteArray upper = rest.toUpper();

    const Part *current = &message;
    bool atMessage = true;
    for (const int number : numbers) {
        const Part *container = current;
        if (!atMessage && current->isMessage() && !current->children.isEmpty())
            container = &current->children.first();
        else if (!atMessage && !current->isMultipart())
            return false;
        if (container->isMultipart()) {
            if (number > container->children.size())
                return false;
            current = &container->children.at(number - 1);
        } else {
            if (number != 1)
                return false;
            current = container;
        }
        atMessage = false;
    }

    // HEADER, TEXT and the field lists name the parts of a message
    const Part *headerOwner = nullptr;
    if (atMessage)
        headerOwner = current;
    else if (current->isMessage() && !current->children.isEmpty())
        headerOwner = &current->children.first();

    if (upper.isEmpty()) {
        *out = atMessage ? raw : current->body;
        return true;
    }
    if (upper == "MIME") {
        if (atMessage)
            return false;
        *out = current->header.raw();
        return true;
    }
    if (!headerOwner)
        return false;
    if (upper == "HEADER") {
        *out = headerOwner->header.raw();
        return true;
    }
    if (upper == "TEXT") {
        *out = headerOwner->body;
        return true;
    }
    if (upper.startsWith("HEADER.FIELDS.NOT")) {
        *out = headerOwner->header.fields(fieldNames(rest.mid(17)), true);
        return true;
    }
    if (upper.startsWith("HEADER.FIELDS")) {
        *out = headerOwner->header.fields(fieldNames(rest.mid(13)), false);
        return true;
    }
    return false;
}

QDateTime sentDate(const Header &header)
{
    const QByteArray value = simplified(withoutComments(header.value("date")));
    if (value.isEmpty())
        return {};
    QDateTime date = QDateTime::fromString(QString::fromLatin1(value), Qt::RFC2822Date);
    if (!date.isValid()) {
        // Without the day of the week
        const int comma = value.indexOf(',');
        if (comma >= 0)
            date = QDateTime::fromString(QString::fromLatin1(value.mid(comma + 1).trimmed()), Qt::RFC2822Date);
    }
    return date;
}

}
}
