declare const _default: (modules: {
    typescript: typeof import("typescript/lib/tsserverlibrary");
}) => {
    create(info: import("typescript/lib/tsserverlibrary").server.PluginCreateInfo): any;
    onConfigurationChanged(_config: any): void;
    decorate(languageService: import("typescript/lib/tsserverlibrary").LanguageService): import("typescript/lib/tsserverlibrary").LanguageService;
};
export = _default;
