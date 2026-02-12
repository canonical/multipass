/*
 * Copyright (C) Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Antoni Bertolin <antoni.monferrer@canonical.com>
 *
 */
use proc_macro::TokenStream;
use proc_macro2::Span;
use quote::quote;
use syn::{LitStr, parse_macro_input};

#[proc_macro]
pub fn make_word_array(input: TokenStream) -> TokenStream {
    // Parse filename as string literal
    let filename = parse_macro_input!(input as LitStr).value();

    // Read file contents
    let content = std::fs::read_to_string(&filename).expect("Could not read file");

    // Split & reformat as string literals for quote!
    let words: Vec<syn::LitStr> = content
        .split_whitespace()
        .map(|word| syn::LitStr::new(word, Span::call_site()))
        .collect();

    let output = quote! {
        [ #( #words ),* ]
    };
    output.into()
}
