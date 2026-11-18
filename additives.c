#include "librefood.h"

/*
 * Common EU food additives (E-numbers) → names. Not exhaustive: unknown
 * codes are still shown, just without a name. Sub-variants such as
 * "e322i" fall back to their base code ("e322").
 */
static const struct { const char *code; const char *name; } ADDITIVES[] = {
    /* Colours */
    { "e100", "Curcumin" },            { "e101", "Riboflavin" },
    { "e102", "Tartrazine" },          { "e104", "Quinoline yellow" },
    { "e110", "Sunset yellow FCF" },   { "e120", "Carmine" },
    { "e122", "Azorubine" },           { "e124", "Ponceau 4R" },
    { "e129", "Allura red AC" },       { "e131", "Patent blue V" },
    { "e132", "Indigo carmine" },      { "e133", "Brilliant blue FCF" },
    { "e140", "Chlorophylls" },        { "e141", "Copper chlorophylls" },
    { "e150", "Caramel colour" },      { "e150a", "Plain caramel" },
    { "e150b", "Caustic sulphite caramel" },
    { "e150c", "Ammonia caramel" },    { "e150d", "Sulphite ammonia caramel" },
    { "e151", "Brilliant black BN" },  { "e153", "Vegetable carbon" },
    { "e160a", "Carotenes" },          { "e160b", "Annatto" },
    { "e160c", "Paprika extract" },    { "e160d", "Lycopene" },
    { "e161b", "Lutein" },             { "e162", "Beetroot red" },
    { "e163", "Anthocyanins" },        { "e170", "Calcium carbonate" },
    { "e171", "Titanium dioxide" },    { "e172", "Iron oxides" },
    /* Preservatives */
    { "e200", "Sorbic acid" },         { "e202", "Potassium sorbate" },
    { "e210", "Benzoic acid" },        { "e211", "Sodium benzoate" },
    { "e220", "Sulphur dioxide" },     { "e223", "Sodium metabisulphite" },
    { "e224", "Potassium metabisulphite" },
    { "e250", "Sodium nitrite" },      { "e251", "Sodium nitrate" },
    { "e252", "Potassium nitrate" },   { "e260", "Acetic acid" },
    { "e262", "Sodium acetates" },     { "e270", "Lactic acid" },
    { "e280", "Propionic acid" },      { "e282", "Calcium propionate" },
    { "e290", "Carbon dioxide" },      { "e296", "Malic acid" },
    /* Antioxidants & acidity regulators */
    { "e300", "Ascorbic acid" },       { "e301", "Sodium ascorbate" },
    { "e306", "Tocopherol-rich extract" },
    { "e307", "Alpha-tocopherol" },    { "e310", "Propyl gallate" },
    { "e316", "Sodium erythorbate" },  { "e319", "TBHQ" },
    { "e320", "BHA" },                 { "e321", "BHT" },
    { "e322", "Lecithins" },           { "e325", "Sodium lactate" },
    { "e327", "Calcium lactate" },     { "e330", "Citric acid" },
    { "e331", "Sodium citrates" },     { "e332", "Potassium citrates" },
    { "e333", "Calcium citrates" },    { "e334", "Tartaric acid" },
    { "e338", "Phosphoric acid" },     { "e339", "Sodium phosphates" },
    { "e340", "Potassium phosphates" },{ "e341", "Calcium phosphates" },
    { "e350", "Sodium malates" },      { "e392", "Rosemary extract" },
    /* Thickeners, stabilisers, emulsifiers */
    { "e400", "Alginic acid" },        { "e401", "Sodium alginate" },
    { "e406", "Agar" },                { "e407", "Carrageenan" },
    { "e410", "Locust bean gum" },     { "e412", "Guar gum" },
    { "e414", "Gum arabic" },          { "e415", "Xanthan gum" },
    { "e418", "Gellan gum" },          { "e420", "Sorbitol" },
    { "e421", "Mannitol" },            { "e422", "Glycerol" },
    { "e440", "Pectins" },             { "e450", "Diphosphates" },
    { "e451", "Triphosphates" },       { "e452", "Polyphosphates" },
    { "e460", "Cellulose" },           { "e461", "Methyl cellulose" },
    { "e466", "Carboxymethyl cellulose" },
    { "e471", "Mono- and diglycerides of fatty acids" },
    { "e472", "Esters of mono- and diglycerides" },
    { "e472e", "DATEM" },
    { "e475", "Polyglycerol esters of fatty acids" },
    { "e476", "Polyglycerol polyricinoleate" },
    { "e481", "Sodium stearoyl lactylate" },
    { "e491", "Sorbitan monostearate" },
    /* Acids, bases, anti-caking */
    { "e500", "Sodium carbonates" },   { "e501", "Potassium carbonates" },
    { "e503", "Ammonium carbonates" }, { "e504", "Magnesium carbonates" },
    { "e507", "Hydrochloric acid" },   { "e508", "Potassium chloride" },
    { "e509", "Calcium chloride" },    { "e516", "Calcium sulphate" },
    { "e524", "Sodium hydroxide" },    { "e551", "Silicon dioxide" },
    { "e570", "Fatty acids" },         { "e575", "Glucono-delta-lactone" },
    /* Flavour enhancers */
    { "e620", "Glutamic acid" },       { "e621", "Monosodium glutamate" },
    { "e627", "Disodium guanylate" },  { "e631", "Disodium inosinate" },
    { "e635", "Disodium ribonucleotides" },
    /* Glazing agents, gases, sweeteners */
    { "e900", "Dimethylpolysiloxane" },{ "e901", "Beeswax" },
    { "e903", "Carnauba wax" },        { "e904", "Shellac" },
    { "e920", "L-cysteine" },          { "e941", "Nitrogen" },
    { "e950", "Acesulfame K" },        { "e951", "Aspartame" },
    { "e952", "Cyclamates" },          { "e954", "Saccharin" },
    { "e955", "Sucralose" },           { "e960", "Steviol glycosides" },
    { "e965", "Maltitol" },            { "e967", "Xylitol" },
    { "e968", "Erythritol" },
    /* Modified starches & misc */
    { "e1400", "Dextrin" },            { "e1404", "Oxidised starch" },
    { "e1412", "Distarch phosphate" },
    { "e1414", "Acetylated distarch phosphate" },
    { "e1420", "Acetylated starch" },
    { "e1422", "Acetylated distarch adipate" },
    { "e1442", "Hydroxypropyl distarch phosphate" },
    { "e1450", "Starch sodium octenyl succinate" },
    { "e1520", "Propylene glycol" },
};

static const char *lookup(const char *code) {
    for (gsize i = 0; i < G_N_ELEMENTS(ADDITIVES); i++)
        if (g_ascii_strcasecmp(ADDITIVES[i].code, code) == 0)
            return ADDITIVES[i].name;
    return NULL;
}

const char *additive_name(const char *code) {
    if (!code || !*code) return NULL;
    const char *name = lookup(code);
    if (name) return name;
    /* Strip variant suffixes: e322i → e322, e150d stays (listed), e500ii → e500 */
    char *base = g_strdup(code);
    gsize n = strlen(base);
    while (n > 1 && g_ascii_isalpha(base[n - 1])) {
        base[--n] = '\0';
        if ((name = lookup(base))) break;
    }
    g_free(base);
    return name;
}

char *additive_display(const char *code) {
    if (!code) return g_strdup("");
    char *upper = g_strdup(code);
    if (upper[0]) upper[0] = g_ascii_toupper(upper[0]);   /* e322i → E322i */
    const char *name = additive_name(code);
    char *out = name ? g_strdup_printf("%s · %s", upper, name) : g_strdup(upper);
    g_free(upper);
    return out;
}
