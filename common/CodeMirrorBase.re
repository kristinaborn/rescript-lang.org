/*
    CodeMirrorBase is a clientside only component that can not be used in a isomorphic environment.
    Within our Next project, you will be required to use the <CodeMirror/> component instead, which
    will lazily import the CodeMirrorBase via Next's `dynamic` component loading mechanisms.

    ! If you load this component in a Next page without using dynamic loading, you will get a SSR error !

    This file is providing the core functionality and logic of our CodeMirror instances.
 */

%raw
{|
import "codemirror/lib/codemirror.css";
import "../styles/cm.css";

if (typeof window !== "undefined" && typeof window.navigator !== "undefined") {
  require("codemirror/mode/javascript/javascript");
  require("../plugins/cm-reason-mode");
}
|};

/* The module for interacting with the imperative CodeMirror API */
module CM = {
  type t;

  let errorGutterId = "errors";

  module Options = {
    [@bs.deriving {abstract: light}]
    type t = {
      theme: string,
      [@bs.optional]
      gutters: array(string),
      mode: string,
      [@bs.optional]
      lineNumbers: bool,
      [@bs.optional]
      readOnly: bool,
      [@bs.optional]
      lineWrapping: bool,
    };
  };

  [@bs.module "codemirror"]
  external fromTextArea: (Dom.element, Options.t) => t = "fromTextArea";

  [@bs.send]
  external getScrollerElement: t => Dom.element = "getScrollerElement";

  [@bs.send]
  external getWrapperElement: t => Dom.element = "getWrapperElement";

  [@bs.send] external refresh: t => unit = "refresh";

  [@bs.send]
  external onChange:
    (t, [@bs.as "change"] _, [@bs.uncurry] (t => unit)) => unit =
    "on";
  [@bs.send] external toTextArea: t => unit = "toTextArea";

  [@bs.send] external setValue: (t, string) => unit = "setValue";

  [@bs.send] external getValue: t => string = "getValue";

  [@bs.send]
  external operation: (t, [@bs.uncurry] (unit => unit)) => unit = "operation";

  [@bs.send]
  external setGutterMarker: (t, int, string, Dom.element) => unit =
    "setGutterMarker";

  [@bs.send] external clearGutter: (t, string) => unit;

  type markPos = {
    line: int,
    ch: int,
  };

  module TextMarker = {
    type t;

    [@bs.send] external clear: t => unit = "clear";
  };

  module MarkTextOption = {
    type t;

    module Attr = {
      type t;
      [@bs.obj] external make: (~id: string=?, unit) => t;
    };

    [@bs.obj]
    external make: (~className: string=?, ~attributes: Attr.t, unit) => t;
  };

  [@bs.send]
  external markText: (t, markPos, markPos, MarkTextOption.t) => TextMarker.t =
    "markText";
};

module DomUtil = {
  module Event = {
    type t;

    [@bs.get] external target: t => Dom.element = "target";
  };

  [@bs.val] [@bs.scope "document"]
  external createElement: string => Dom.element = "createElement";

  [@bs.send]
  external appendChild: (Dom.element, Dom.element) => unit = "appendChild";

  [@bs.set] [@bs.scope "style"]
  external setMinHeight: (Dom.element, string) => unit = "minHeight";

  [@bs.set] [@bs.scope "style"]
  external setMaxHeight: (Dom.element, string) => unit = "maxHeight";

  [@bs.set] [@bs.scope "style"]
  external setDisplay: (Dom.element, string) => unit = "display";

  [@bs.set] [@bs.scope "style"]
  external setTop: (Dom.element, string) => unit = "top";

  [@bs.set] [@bs.scope "style"]
  external setLeft: (Dom.element, string) => unit = "left";

  [@bs.set] external setInnerHTML: (Dom.element, string) => unit = "innerHTML";

  [@bs.set] external setId: (Dom.element, string) => unit = "id";
  [@bs.set] external setClassName: (Dom.element, string) => unit = "className";

  [@bs.set]
  external setOnMouseOver: (Dom.element, Event.t => unit) => unit =
    "onmouseover";

  [@bs.set]
  external setOnMouseLeave: (Dom.element, Event.t => unit) => unit =
    "onmouseleave";

  [@bs.set]
  external setOnMouseOut: (Dom.element, Event.t => unit) => unit =
    "onmouseout";

  [@bs.get] external getId: Dom.element => string = "id";

  type clientRect = {
    x: int,
    y: int,
    width: int,
    height: int,
  };

  [@bs.send]
  external getBoundingClientRect: Dom.element => clientRect =
    "getBoundingClientRect";
};

module Error = {
  type kind = [ | `Error | `Warning];

  type t = {
    row: int,
    column: int,
    endRow: int,
    endColumn: int,
    text: string,
    kind,
  };
};

module Props = {
  [@bs.deriving {abstract: light}]
  type t = {
    [@bs.optional]
    errors: array(Error.t),
    [@bs.optional]
    minHeight: string, // minHeight of the scroller element
    [@bs.optional]
    maxHeight: string, // maxHeight of the scroller element
    [@bs.optional]
    className: string,
    [@bs.optional]
    style: ReactDOMRe.Style.t,
    value: string,
    [@bs.optional]
    onChange: string => unit,
    [@bs.optional]
    onMarkerFocus: ((int, int)) => unit, // (row, column)
    [@bs.optional]
    onMarkerFocusLeave: ((int, int)) => unit, // (row, column)
    options: CM.Options.t,
  };
};

module GutterMarker = {
  // Note: this is not a React component
  let make =
      (
        ~rowCol: (int, int), // row, col
        ~kind: Error.kind,
        ~wrapper: Dom.element,
        (),
      )
      : Dom.element => {
    open DomUtil;

    let marker = createElement("div");
    let colorClass =
      switch (kind) {
      | `Warning => "text-gold bg-gold-15"
      | `Error => "text-fire bg-fire-15"
      };

    let (row, col) = rowCol;
    marker->setId({j|gutter-marker_$row-$col|j});
    marker->setClassName(
      "flex items-center justify-center text-14 text-center ml-1 h-6 font-bold hover:cursor-pointer "
      ++ colorClass,
    );
    marker->setInnerHTML("!");

    marker;
  };
};

type state = {marked: array(CM.TextMarker.t)};

let clearMarks = (state: state): unit => {
  Belt.Array.forEach(state.marked, mark => {mark->CM.TextMarker.clear});
};

let extractRowColFromId = (id: string): option((int, int)) => {
  switch (Js.String2.split(id, "_")) {
  | [|_, rowColStr|] =>
    switch (Js.String2.split(rowColStr, "-")) {
    | [|rowStr, colStr|] =>
      let row = Belt.Int.fromString(rowStr);
      let col = Belt.Int.fromString(colStr);
      switch (row, col) {
      | (Some(row), Some(col)) => Some((row, col))
      | _ => None
      };
    | _ => None
    }
  | _ => None
  };
};

let updateErrors =
    (
      ~state: state,
      ~onMarkerFocus=?,
      ~onMarkerFocusLeave=?,
      ~cm: CM.t,
      errors,
    ) => {
  clearMarks(state);
  cm->CM.(clearGutter(errorGutterId));

  let wrapper = cm->CM.getWrapperElement;

  Belt.Array.forEachWithIndex(
    errors,
    (idx, e) => {
      open DomUtil;
      open Error;

      let marker =
        GutterMarker.make(
          ~rowCol=(e.row, e.column),
          ~kind=e.kind,
          ~wrapper,
          (),
        );

      wrapper->appendChild(marker);

      // CodeMirrors line numbers are (strangely enough) zero based
      let row = e.row - 1;
      let endRow = e.endRow - 1;

      cm->CM.setGutterMarker(row, CM.errorGutterId, marker);

      let from = {CM.line: row, ch: e.column};
      let to_ = {CM.line: endRow, ch: e.endColumn};

      let markTextColor =
        switch (e.kind) {
        | `Error => "border-fire"
        | `Warning => "border-gold"
        };

      cm
      ->CM.markText(
          from,
          to_,
          CM.MarkTextOption.make(
            ~className=
              "border-b border-dotted hover:cursor-pointer " ++ markTextColor,
            ~attributes=
              CM.MarkTextOption.Attr.make(
                ~id=
                  "text-marker_"
                  ++ Belt.Int.toString(e.row)
                  ++ "-"
                  ++ Belt.Int.toString(e.column)
                  ++ "",
                (),
              ),
            (),
          ),
        )
      ->Js.Array2.push(state.marked, _)
      ->ignore;
      ();
    },
  );

  let isMarkerId = id =>
    Js.String2.startsWith(id, "gutter-marker")
    || Js.String2.startsWith(id, "text-marker");

  wrapper->DomUtil.(
             setOnMouseOver(evt => {
               let target = Event.target(evt);

               let id = getId(target);
               if (isMarkerId(id)) {
                 switch (extractRowColFromId(id)) {
                 | Some(rowCol) =>
                   Belt.Option.forEach(onMarkerFocus, cb => cb(rowCol))
                 | None => ()
                 };
               };
             })
           );

  wrapper->DomUtil.(
             setOnMouseOut(evt => {
               let target = Event.target(evt);

               let id = getId(target);
               if (isMarkerId(id)) {
                 switch (extractRowColFromId(id)) {
                 | Some(rowCol) =>
                   Belt.Option.forEach(onMarkerFocusLeave, cb => cb(rowCol))
                 | None => ()
                 };
               };
             })
           );
};

let default = (props: Props.t): React.element => {
  let inputElement = React.useRef(Js.Nullable.null);
  let cmRef: React.Ref.t(option(CM.t)) = React.useRef(None);
  let cmStateRef = React.useRef({marked: [||]});

  // Destruct all our props here
  let minHeight = props->Props.minHeight;
  let maxHeight = props->Props.maxHeight;
  let onChange = props->Props.onChange;
  let value = props->Props.value;
  let errors = Belt.Option.getWithDefault(props->Props.errors, [||]);
  let className = props->Props.className;
  let style = props->Props.style;
  let cmOptions = props->Props.options;

  let onMarkerFocus = props->Props.onMarkerFocus;
  let onMarkerFocusLeave = props->Props.onMarkerFocusLeave;

  React.useEffect0(() => {
    switch (inputElement->React.Ref.current->Js.Nullable.toOption) {
    | Some(input) =>
      let options =
        CM.Options.t(
          ~theme="material",
          ~gutters=[|CM.errorGutterId, "CodeMirror-linenumbers"|],
          ~mode=cmOptions->CM.Options.mode,
          ~lineWrapping=
            Belt.Option.getWithDefault(
              cmOptions->CM.Options.lineWrapping,
              false,
            ),
          ~readOnly=
            Belt.Option.getWithDefault(cmOptions->CM.Options.readOnly, false),
          ~lineNumbers=true,
          (),
        );
      let cm = CM.fromTextArea(input, options);
      /*Js.log2("cm", cm);*/

      Belt.Option.forEach(minHeight, minHeight =>
        cm->CM.getScrollerElement->DomUtil.setMinHeight(minHeight)
      );

      Belt.Option.forEach(maxHeight, maxHeight =>
        cm->CM.getScrollerElement->DomUtil.setMaxHeight(maxHeight)
      );

      Belt.Option.forEach(onChange, onValueChange => {
        cm->CM.onChange(instance => {onValueChange(instance->CM.getValue)})
      });

      // For some reason, injecting value with the options doesn't work
      // so we need to set the initial value imperatively
      cm->CM.setValue(value);

      React.Ref.setCurrent(cmRef, Some(cm));

      let cleanup = () => {
        /*Js.log2("cleanup", options->CM.Options.mode);*/

        // This will destroy the CM instance
        cm->CM.toTextArea;
        React.Ref.setCurrent(cmRef, None);
      };

      Some(cleanup);
    | None => None
    }
  });

  /*
     Previously we did this in a useEffect([|value|) setup, but
     this issues for syncing up the current editor value state
     with the passed value prop.

     Example: Let's assume you press a format code button for a
     piece of code that formats to the same value as the previously
     passed value prop. Even though the source code looks different
     in the editor (as observed via getValue) it doesn't recognize
     that there is an actual change.

     By checking if the local state of the CM instance is different
     to the input value, we can sync up both states accordingly
   */
  switch (cmRef->React.Ref.current) {
  | Some(cm) =>
    if (CM.getValue(cm) === value) {
      ();
    } else {
      let state = cmStateRef->React.Ref.current;
      cm->CM.operation(() => {
        updateErrors(
          ~onMarkerFocus?,
          ~onMarkerFocusLeave?,
          ~state,
          ~cm,
          errors,
        );
      });
      cm->CM.setValue(value);
    }
  | None => ()
  };

  /*
      This is required since the incoming error
      array is not guaranteed to be the same instance,
      so we need to make a single string that React's
      useEffect is able to act on for equality checks
   */
  let errorsFingerprint =
    Belt.Array.map(
      errors,
      e => {
        let {Error.row, column, text} = e;
        {j|$row-$column-$text|j};
      },
    )
    ->Js.Array2.joinWith(";");

  React.useEffect1(
    () => {
      let state = cmStateRef->React.Ref.current;
      switch (cmRef->React.Ref.current) {
      | Some(cm) =>
        cm->CM.operation(() => {
          updateErrors(
            ~onMarkerFocus?,
            ~onMarkerFocusLeave?,
            ~state,
            ~cm,
            errors,
          );
        })
      | None => ()
      };
      None;
    },
    [|errorsFingerprint|],
  );

  <div ?className ?style>
    <textarea className="hidden" ref={ReactDOMRe.Ref.domRef(inputElement)} />
  </div>;
};
