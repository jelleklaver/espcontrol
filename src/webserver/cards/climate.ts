import {
    cardContractAllowInSubpage,
    cardContractCard,
    cardContractCardLabel,
    cardContractDefaultConfig,
    cardContractDomains,
    cardContractHidden,
    cardContractPickerKey,
} from "../generated/card_contract";
import { iconSlug } from "../application/ui_primitives";
import type { CardRegistry, CardUiServices } from "../application/card_registry";
import type { ConfigModalTabOptionsFeature } from "../application/config_modal_tab_options";
import type { ConfigAccessClimateAlarmOptionsFeature } from "../application/config_access_climate_alarm_options";
import type { ClockBarFeature } from "../application/clock_bar_state";
import type { ButtonSettingsRenderQueueFeature } from "../application/button_settings_render_queue";
import type { ControlsFieldsFeature } from "../application/controls_fields";
export function registerClimateCardTypes(
    registry: CardRegistry,
    modalTabs: ConfigModalTabOptionsFeature,
    accessOptions: ConfigAccessClimateAlarmOptionsFeature,
    clockBar: Pick<ClockBarFeature, "temperatureUnitSymbol">,
    renderQueue: ButtonSettingsRenderQueueFeature,
    fields: ControlsFieldsFeature,
    cardUi: CardUiServices,
): void {
    const { renderButtonSettings } = cardUi;
    const { cardBadgeLabelHtml, cardSensorPreviewHtml, condField } = fields;
    const { temperatureUnitSymbol } = clockBar;
    const {
        climateControlTabDefinitions,
        climateControlTabs,
        setClimateControlTabs,
        renderModalTabSettings,
    } = modalTabs;
    const {
        normalizeClimateOptions,
        climateCardIsLocal,
        climateLabelDisplayMode,
        setClimateLabelDisplayMode,
        climateNumberDisplayMode,
        setClimateNumberDisplayMode,
        climateTemperatureStep,
        setClimateTemperatureStep,
        parseClimatePrecisionConfig,
        climatePrecisionConfig,
    } = accessOptions;
    // Climate card: thermostat status plus full-screen climate controls.
    const CLIMATE_CARD_METADATA: any = {
        source: {
            label: "Source",
            options: [
                ["ha", "Home Assistant"],
                ["local", "Local Climate"],
            ],
            value: function (this: any, b?: any) {
                return climateCardIsLocal(b) ? "local" : "ha";
            },
        },
        entity: {
            label: "Climate Entity",
            idSuffix: "entity",
            placeholder: "e.g. climate.living_room",
            domains: function (this: any) { return cardContractDomains("climate"); },
            bindName: "entity",
            rerender: true,
            requiredMessage: "Add a climate entity before saving.",
        },
        labelDisplay: {
            label: "Label Display",
            options: [
                ["label", "Label"],
                ["status", "Status"],
                ["actual", "Actual"],
                ["target", "Target"],
            ],
        },
        numberDisplay: {
            label: "Icon & Temperatures",
            options: [
                ["icon", "Icon"],
                ["actual", "Actual"],
                ["target", "Target"],
            ],
        },
        temperatureStep: {
            label: "Temperature Step",
            options: [
                ["1", "1 degree"],
                ["0.5", "0.5 degree"],
            ],
        },
        largeNumbers: {
            label: "Large Temperature Numbers",
            idSuffix: "large-temperature-numbers",
            supported: function (this: any, b?: any) {
                return climateNumberDisplayMode(b) !== "icon";
            },
        },
        preview: {
            badge: "thermostat",
        },
    };
    function renderClimateLocalEntityField(this: any, panel?: any, b?: any, helpers?: any) {
        var wrap: any = document.createElement("div");
        wrap.className = "sp-field";
        wrap.appendChild(helpers.fieldLabel("Climate Entity", helpers.idPrefix + "climate-local-entity"));
        var loadingDiv: any = document.createElement("div");
        loadingDiv.className = "sp-hint";
        loadingDiv.textContent = "Loading local climate entities…";
        wrap.appendChild(loadingDiv);
        panel.appendChild(wrap);
        function buildDropdown(this: any, climates?: any) {
            wrap.innerHTML = "";
            wrap.appendChild(helpers.fieldLabel("Climate Entity", helpers.idPrefix + "climate-local-entity"));
            if (!climates.length) {
                var noneDiv: any = document.createElement("div");
                noneDiv.className = "sp-banner sp-error";
                noneDiv.textContent = "No local climate entities found on this device.";
                wrap.appendChild(noneDiv);
                return;
            }
            var sel: any = document.createElement("select");
            sel.className = "sp-select";
            sel.id = helpers.idPrefix + "climate-local-entity";
            var placeholder: any = document.createElement("option");
            placeholder.value = "";
            placeholder.textContent = "Select a local climate…";
            sel.appendChild(placeholder);
            climates.forEach(function (this: any, c?: any) {
                var opt: any = document.createElement("option");
                opt.value = c.key;
                opt.textContent = c.name || c.key;
                if (c.key === b.entity)
                    opt.selected = true;
                sel.appendChild(opt);
            });
            sel.addEventListener("change", function (this: any) {
                var key: any = sel.value;
                b.entity = key;
                helpers.saveField("entity", key);
                if (!b.label) {
                    var match: any = climates.find(function (this: any, c?: any) { return c.key === key; });
                    if (match) {
                        b.label = match.name || key;
                        helpers.saveField("label", b.label);
                    }
                }
            });
            wrap.appendChild(sel);
            helpers.requireField(sel, "Select a local climate entity before saving.");
        }
        function buildManualInput(this: any) {
            wrap.innerHTML = "";
            wrap.className = "sp-local-picker-fallback";
            var errDiv: any = document.createElement("div");
            errDiv.className = "sp-banner sp-error";
            errDiv.textContent = "Could not reach device. Enter the climate object_id manually.";
            wrap.appendChild(errDiv);
            wrap.appendChild(helpers.fieldLabel("Climate Object ID", helpers.idPrefix + "climate-local-entity-key"));
            var keyInp: any = helpers.textInput(helpers.idPrefix + "climate-local-entity-key", b.entity, "e.g. ch_climate");
            wrap.appendChild(keyInp);
            helpers.bindField(keyInp, "entity", true);
            helpers.requireField(keyInp, "Add a climate object_id before saving.");
        }
        fetch("/local_climates")
            .then(function (this: any, resp?: any) {
            if (!resp.ok)
                throw new Error("HTTP " + resp.status);
            return resp.json();
        })
            .then(function (this: any, data?: any) {
            buildDropdown(data);
        })
            .catch(function (this: any) {
            buildManualInput();
        });
    }
    registry.register("climate", {
        label: function (this: any) { return cardContractCardLabel("climate"); },
        allowInSubpage: function (this: any) { return cardContractAllowInSubpage("climate"); },
        pickerKey: function (this: any) { return cardContractPickerKey("climate"); },
        hidden: function (this: any) { return cardContractHidden("climate"); },
        hideLabel: true,
        labelPlaceholder: "e.g. Living Room",
        defaultConfig: function (this: any) { return cardContractDefaultConfig("climate"); },
        cardMetadata: CLIMATE_CARD_METADATA,
        onSelect: function (this: any, b?: any) {
            b.entity = "";
            b.label = "Climate";
            b.sensor = "";
            b.unit = "";
            b.type = "climate_control";
            b.precision = "";
            b.icon = "Thermostat";
            b.icon_on = "Auto";
            b.options = "";
        },
        renderSettings: function (this: any, panel?: any, b?: any, slot?: any, helpers?: any) {
            if (b.type !== "climate_control") {
                b.type = "climate_control";
                helpers.saveField("type", b.type);
            }
            b.unit = "";
            var sourceControl: any = helpers.renderCardSegmentControl(panel, b, helpers, {
                segment: Object.assign({}, CLIMATE_CARD_METADATA.source, {
                    onSelect: function (this: any, button?: any, cardHelpers?: any, value?: any) {
                        setSource(value);
                    },
                }),
            });
            var sourceButtons: any = sourceControl.buttons;
            sourceButtons.ha.classList.toggle("active", !climateCardIsLocal(b));
            sourceButtons.local.classList.toggle("active", climateCardIsLocal(b));
            function setSource(this: any, value?: any) {
                var local: any = value === "local";
                if (local === climateCardIsLocal(b))
                    return;
                b.entity = "";
                b.sensor = local ? "local" : "";
                helpers.saveField("entity", b.entity);
                helpers.saveField("sensor", b.sensor);
                renderButtonSettings();
            }
            if (!climateCardIsLocal(b))
                b.sensor = "";
            if (!b.icon)
                b.icon = "Thermostat";
            if (!b.icon_on)
                b.icon_on = "Auto";
            var climateConfig: any = parseClimatePrecisionConfig(b.precision);
            var normalizedPrecision: any = climatePrecisionConfig(climateConfig.precision, climateConfig.min, climateConfig.max);
            if (b.precision !== normalizedPrecision) {
                b.precision = normalizedPrecision;
                helpers.saveField("precision", normalizedPrecision);
            }
            if (climateCardIsLocal(b)) {
                renderClimateLocalEntityField(panel, b, helpers);
            } else {
                helpers.renderCardEntityField(panel, b, helpers, CLIMATE_CARD_METADATA);
            }
            var modalTabsDisclosure: any = helpers.disclosureSection("Modal Settings", helpers.idPrefix + "climate-modal-tabs", b._modalSettingsOpen === true);
            renderModalTabSettings(modalTabsDisclosure.section, b, helpers, {
                definitions: climateControlTabDefinitions,
                tabs: climateControlTabs,
                normalizeOptions: function (this: any, options?: any) { return normalizeClimateOptions(options, true); },
                setTabs: setClimateControlTabs,
                idPrefix: "climate-tab-",
                hideHeading: true,
            });
            var labelField: any = condField();
            labelField.classList.add("sp-climate-settings-gap");
            helpers.renderCardTextField(labelField, b, helpers, {
                label: "Label",
                idSuffix: "label",
                field: "label",
                placeholder: "Climate",
                rerender: true,
            });
            function syncLabelField(this: any) {
                labelField.classList.toggle("sp-visible", climateLabelDisplayMode(b) === "label");
            }
            var cardSettingsDisclosure: any = helpers.disclosureSection("Card Settings", helpers.idPrefix + "climate-card-settings", false);
            var cardSettings: any = cardSettingsDisclosure.section;
            helpers.renderCardSegmentControl(cardSettings, b, helpers, {
                segment: Object.assign({}, CLIMATE_CARD_METADATA.numberDisplay, {
                    value: function (this: any) { return climateNumberDisplayMode(b); },
                    onSelect: function (this: any, button?: any, cardHelpers?: any, value?: any) {
                        setClimateNumberDisplayMode(button, value);
                        cardHelpers.saveField("options", button.options);
                        syncIconFields();
                        renderQueue.schedule();
                    },
                }),
            });
            var iconFields: any = condField();
            iconFields.classList.add("sp-climate-settings-gap");
            helpers.renderCardIconPicker(iconFields, b, helpers, {
                pickerIdSuffix: "climate-icon-picker",
                idSuffix: "climate-icon",
                field: "icon",
                fallback: "Thermostat",
                label: "Off Icon",
                onChange: function (this: any) { renderQueue.schedule(); },
            });
            helpers.renderCardIconPicker(iconFields, b, helpers, {
                pickerIdSuffix: "climate-icon-on-picker",
                idSuffix: "climate-icon-on",
                field: "icon_on",
                fallback: "Auto",
                label: "On Icon",
                onChange: function (this: any) { renderQueue.schedule(); },
            });
            function syncIconFields(this: any) {
                iconFields.classList.toggle("sp-visible", climateNumberDisplayMode(b) === "icon");
            }
            syncIconFields();
            cardSettings.appendChild(iconFields);
            helpers.renderCardSegmentControl(cardSettings, b, helpers, {
                segment: Object.assign({}, CLIMATE_CARD_METADATA.labelDisplay, {
                    value: function (this: any) { return climateLabelDisplayMode(b); },
                    onSelect: function (this: any, button?: any, cardHelpers?: any, value?: any) {
                        setClimateLabelDisplayMode(button, value);
                        cardHelpers.saveField("options", button.options);
                        syncLabelField();
                        renderQueue.schedule();
                    },
                }),
            });
            syncLabelField();
            cardSettings.appendChild(labelField);
            var precisionField: any = helpers.selectField("Temperature Settings", helpers.idPrefix + "climate-precision", [
                ["", "10"],
                ["1", "10.2"],
            ], climateConfig.precision);
            var precision: any = precisionField.select;
            function saveClimateAdvancedSettings(this: any) {
                b.precision = climatePrecisionConfig(precision.value, minInp.value, maxInp.value);
                helpers.saveField("precision", b.precision);
                renderQueue.schedule();
            }
            precision.addEventListener("change", saveClimateAdvancedSettings);
            var stepField: any = helpers.selectField(CLIMATE_CARD_METADATA.temperatureStep.label, helpers.idPrefix + "climate-temperature-step", CLIMATE_CARD_METADATA.temperatureStep.options, climateTemperatureStep(b));
            stepField.select.addEventListener("change", function (this: any) {
                setClimateTemperatureStep(b, stepField.select.value);
                helpers.saveField("options", b.options);
                renderQueue.schedule();
            });
            helpers.renderCardLargeNumbersToggle(cardSettings, b, helpers, CLIMATE_CARD_METADATA);
            panel.appendChild(cardSettingsDisclosure.panel);
            panel.appendChild(modalTabsDisclosure.panel);
            var advancedDisclosure: any = helpers.disclosureSection("Advanced", helpers.idPrefix + "climate-advanced", false);
            var advanced: any = advancedDisclosure.section;
            advanced.appendChild(precisionField.field);
            advanced.appendChild(stepField.field);
            var minField: any = helpers.textField("Minimum Temperature", helpers.idPrefix + "climate-min", climateConfig.min, "e.g. -25");
            var minInp: any = minField.input;
            minInp.inputMode = "decimal";
            advanced.appendChild(minField.field);
            var maxField: any = helpers.textField("Maximum Temperature", helpers.idPrefix + "climate-max", climateConfig.max, "e.g. 5");
            var maxInp: any = maxField.input;
            maxInp.inputMode = "decimal";
            advanced.appendChild(maxField.field);
            minInp.addEventListener("change", saveClimateAdvancedSettings);
            maxInp.addEventListener("change", saveClimateAdvancedSettings);
            panel.appendChild(advancedDisclosure.panel);
        },
        renderPreview: function (this: any, b?: any, helpers?: any) {
            var climateConfig: any = parseClimatePrecisionConfig(b.precision);
            var prec: any = parseInt(climateConfig.precision || "0", 10) || 0;
            var unit: any = temperatureUnitSymbol();
            var actualVal: any = (21).toFixed(prec);
            var targetVal: any = (20).toFixed(prec);
            var numberMode: any = climateNumberDisplayMode(b);
            var numberVal: any = numberMode === "actual" ? actualVal : targetVal;
            var labelMode: any = climateLabelDisplayMode(b);
            var label: any = (b.label && b.label.trim()) || "Climate";
            if (labelMode === "status") {
                label = "Idle";
            }
            else if (labelMode === "actual") {
                label = actualVal + unit;
            }
            else if (labelMode === "target") {
                label = targetVal + unit;
            }
            function climateLabelHtml(this: any) {
                return cardBadgeLabelHtml(helpers, label, CLIMATE_CARD_METADATA.preview.badge);
            }
            if (numberMode === "icon") {
                var iconName: any = b.icon && b.icon !== "Auto" ? b.icon : "Thermostat";
                return {
                    iconHtml: '<span class="sp-btn-icon mdi mdi-' + iconSlug(iconName) + '"></span>',
                    labelHtml: climateLabelHtml(),
                };
            }
            return {
                buttonClass: "sp-climate-temp-card",
                iconHtml: cardSensorPreviewHtml(b, helpers, numberVal, unit),
                labelHtml: climateLabelHtml(),
            };
        },
    });
    registry.register("climate_control", Object.assign({}, registry.definitions.climate, {
        label: function (this: any) { return cardContractCardLabel("climate_control"); },
        allowInSubpage: function (this: any) { return cardContractAllowInSubpage("climate_control"); },
        pickerKey: function (this: any) { return cardContractPickerKey("climate_control"); },
        hidden: function (this: any) { return cardContractHidden("climate_control"); },
        defaultConfig: function (this: any) { return cardContractDefaultConfig("climate_control"); },
    }));
}
