function HandleUnknownMacro(parser, macro) {
    const arg = parser.parse_arg()
    return parser.group(
        parser.raw('<foo>'),
        arg,
        parser.raw('</foo>'),
    )
}

function ToIPA(s) {
    return `/${s}/`;
}

function PreprocessFullEntry() {
    // Nothing.
}
