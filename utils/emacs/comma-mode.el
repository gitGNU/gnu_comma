;;;===-----------------------------------------------------------------------===
;;; comma-mode.el --- major mode for editing Comma code.
;;;
;;; Copyright (C) 2009 Stephen Wilson.
;;;
;;; This program is free software: you can redistribute it and/or modify it
;;; under the terms of the GNU General Public License as published by the Free
;;; Software Foundation, either version 3 of the License, or (at your option)
;;; any later version.
;;;
;;; This program is distributed in the hope that it will be useful, but WITHOUT
;;; ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
;;; FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
;;; more details.
;;;
;;; You should have received a copy of the GNU General Public License along with
;;; this program.  If not, see <http://www.gnu.org/licenses/>.

;;;===-----------------------------------------------------------------------===
;;; Commentary:
;;;
;;; This package provides an Emacs major mode for editing Comma programs.
;;; Current features include:
;;;
;;;   - Syntax highlighting
;;;   - Simple Indentation

;;;===-----------------------------------------------------------------------===
;;; Installation:
;;;
;;; Prerequisites:  Should work with most versions of GNU Emacs but has only
;;; been tested on the 23.X series.
;;;
;;; Put `comma-mode.el' into the `site-lisp' directory of your Emacs
;;; installation for a site wide installation.  Alternatively, you can choose an
;;; arbitrary directory and add the following to your `.emacs':
;;;
;;;   (add-to-list 'load-path "<directory>")
;;;
;;; The following code should be added to your `site-start.el' file in the
;;; `site-lisp' directory or to your `.emacs' in the case of a local install:
;;;
;;;   (require 'comma-mode)
;;;
;;; Also, for better performance you can also byte compile this source file.

;;;===-----------------------------------------------------------------------===
;;; Acknowledgments:
;;;
;;; Some of this code has been derived from or inspired by ada-mode as
;;; distributed with GNU Emacs.

;;;===-----------------------------------------------------------------------===
;;; Customization Options.

(defgroup comma nil
  "Major mode for editing and compiling Comma code."
  :link '(custom-group-link :tag "Font Lock Faces group" font-lock-faces)
  :group 'languages)

(defcustom comma-default-indent 3
  "Default indentation width."
  :type 'integer :group 'comma)

(defcustom comma-return-indent 2
  "Amount to indent then `return' keyword when part of a function declaration."
  :type 'integer :group 'comma)

;;;===-----------------------------------------------------------------------===
;;; Syntax Table.

(defvar comma-mode-syntax-table nil
  "Comma mode syntax table.")

(defun comma-create-syntax-table ()
  "Creates a syntax table for Comma mode and assigns it to
comma-mode-syntax-table."

  (interactive)
  (setq comma-mode-syntax-table (make-syntax-table))

  (modify-syntax-entry ?\" "\"" comma-mode-syntax-table)

  ;; Punctuation entries.
  (modify-syntax-entry ?:  "." comma-mode-syntax-table)
  (modify-syntax-entry ?\; "." comma-mode-syntax-table)
  (modify-syntax-entry ?&  "." comma-mode-syntax-table)
  (modify-syntax-entry ?\| "." comma-mode-syntax-table)
  (modify-syntax-entry ?+  "." comma-mode-syntax-table)
  (modify-syntax-entry ?*  "." comma-mode-syntax-table)
  (modify-syntax-entry ?/  "." comma-mode-syntax-table)
  (modify-syntax-entry ?=  "." comma-mode-syntax-table)
  (modify-syntax-entry ?<  "." comma-mode-syntax-table)
  (modify-syntax-entry ?>  "." comma-mode-syntax-table)
  (modify-syntax-entry ?$  "." comma-mode-syntax-table)
  (modify-syntax-entry ?\[ "." comma-mode-syntax-table)
  (modify-syntax-entry ?\] "." comma-mode-syntax-table)
  (modify-syntax-entry ?\{ "." comma-mode-syntax-table)
  (modify-syntax-entry ?\} "." comma-mode-syntax-table)
  (modify-syntax-entry ?.  "." comma-mode-syntax-table)
  (modify-syntax-entry ?\\ "." comma-mode-syntax-table)
  (modify-syntax-entry ?\' "." comma-mode-syntax-table)

  ;; Underscores are components of Comma symbols.
  (modify-syntax-entry ?_ "_" comma-mode-syntax-table)

  ;; Two hyphens start a comment while one is punctuation.  A newline ends
  ;; comments.
  (modify-syntax-entry ?- ". 12" comma-mode-syntax-table)
  (modify-syntax-entry ?\n  ">   " comma-mode-syntax-table)

  ;; Make parens balance.
  (modify-syntax-entry ?\( "()" comma-mode-syntax-table)
  (modify-syntax-entry ?\) ")(" comma-mode-syntax-table))

;;;===-----------------------------------------------------------------------===
;;; Font Lock Configuration.

(defun comma-font-lock-keywords ()
  (eval-when-compile
    (list

     ;; Reserved words.
     `(,(concat
         "\\<"
         (regexp-opt
          '("abstract" "add" "and" "array" "carrier" "begin" "declare" "domain"
            "else" "elsif" "end" "exception" "for" "function" "generic" "if"
            "import" "in" "inj" "is" "loop" "mod" "of" "out" "or" "others"
            "package" "pragma" "prj" "procedure" "raise" "range" "rem" "return"
            "reverse" "signature" "subtype" "then" "type" "when" "while" "with")
          t)
         "\\>")
       (1 font-lock-keyword-face))

     ;; Highlight end tags, domain, function, procedure, exception, and prama
     ;; names.
     `(,(concat "\\<"
                (regexp-opt '("domain" "end" "function" "package" "pragma"
                              "procedure" "signature" "raise") t)
                "\\>"
                "\\([ \t]+\\)?\\(\\(\\sw\\|[_.]\\)+\\)?")
       (3 font-lock-function-name-face nil t))

     ;; Type declarations.
     `(,(concat "\\<" (regexp-opt '("type" "subtype" "of" "import") t) "\\>"
                "[ \t]+\\(\\(\\sw\\|[_.]\\)+\\)?")
       (2 font-lock-type-face nil t))

     ;; Object decls, formal arguments.
     `(,(concat "\\([a-zA-Z0-9_]+\\)[ \t]*:[^=][ \t]*"
                "\\(in[ \t]\\|out[ \t]\\)*[ \t]*"
                "\\(\\(\\sw\\|[_.]\\)+\\)?")
       (1 font-lock-variable-name-face nil t)
       (3 font-lock-type-face nil t)))))

;;;===-----------------------------------------------------------------------===
;;; Keymap

(defvar comma-keymap (make-sparse-keymap)
  "Local keymap used for Comma mode.")

(defun comma-create-keymap ()
  (define-key comma-keymap "\t" 'comma-indent-current-line)
  comma-keymap)

;;;===-----------------------------------------------------------------------===
;;; Indentation Utility Functions and Macros.

(defun comma-on-first-line ()
  "Returns T if we are on the first line of the buffer."
  (save-excursion (= -1 (forward-line -1))))

(defun comma-on-last-line ()
  "Returns T if we are on the last line of the buffer."
  (save-excursion (= 1 (forward-line 1))))

(defun comma-in-string ()
  "Returns true if point is inside a string literal."
  (save-excursion
    (let* ((limit (point))
           (bol   (progn (beginning-of-line) (point)))
           (state (parse-partial-sexp bol limit)))
      (when (nth 3 state) t))))

(defun comma-in-comment ()
  "Returns true if point is inside a comment."
  (save-excursion
    (let* ((limit (point))
           (bol   (progn (beginning-of-line) (point)))
           (state (parse-partial-sexp bol limit)))
      (when (nth 4 state) t))))

(defun comma-comment-follows ()
  "Returns true if a comment follows point."
  (looking-at "[ \t]*--"))

(defun comma-whiteline-follows ()
  "Returns true if nothing but whitespace follows point up to end
of line."
  (looking-at "[ \t]*$"))

(defun comma-forward-line ()
  "Skips past all empty and comment lines, leaving point at the
start of the first non-empty, non-comment line.  Returns T if
point was moved and NIL otherwise."
  (forward-line 1)
  (unless (comma-on-last-line)
    (catch 'finish
      (while (or (comma-whiteline-follows)
                 (comma-comment-follows))
        (forward-line 1)
        (when (comma-on-last-line)
          (throw 'finish t))))
    t))

(defun comma-back-line ()
  "Skips back across all empty and comment lines, leaving point
at the start of the first non-empty, non-comment line.  Returns T
if point was moved and NIL otherwise."
  (unless (comma-on-first-line)
    (forward-line -1)
    (catch 'finish
      (while (or (comma-whiteline-follows)
                 (comma-comment-follows))
        (forward-line -1)
        (when (comma-on-first-line)
          (throw 'finish t))))
    t))

(defmacro comma-walk-forward (pred &rest body)
  "Moves point forward a line at a time, skipping comments and
whitespace until either PRED returns false or the end of the
buffer is reached.  Invokes BODY with point at the beginning of
each line."
  `(progn
     (comma-forward-line)
     (while (and ,pred
                 (not (comma-on-last-line)))
       (comma-forward-line)
       ,body)))

(defmacro comma-walk-backward (pred &rest body)
  "Moves point backward a line at a time, skipping comments and
whitespace until either PRED returns false or the start of the
buffer is reached.  Invokes BODY with point at the beginning of
each line."
  `(progn
     (comma-back-line)
     (while (and ,pred
                 (not (comma-on-first-line)))
       (comma-back-line)
       ,body)))

(defun comma-seek-back-leading-regex (regex)
  "Walks back a line at a time stopping when REGEX matches the
beginning of a line (or we reach the start of the buffer)."
  (comma-walk-backward
   (progn (back-to-indentation)
          (not (looking-at regex))))
  (not (comma-on-first-line)))

(defun comma-at-start-of-line ()
  "Returns T if point is inside the leading whitespace of a line of text."
  (let* ((pos   (point))
         (start (line-beginning-position))
         (end   (+ start (current-indentation))))
    (and (<= start pos) (<= pos end))))

(defun comma-forward-whitespace (&optional skip-newlines)
  "Moves point across whitespace including newlines if
SKIP-NEWLINES is true.  Returns the new value of point."
  (or (if skip-newlines
          (re-search-forward "[ \t$]*" nil t)
        (re-search-forward "[ \t]*" nil t))
      (point)))

(defun comma-scan-identifier ()
  "Parses a single identifier following point and returns a
string representation or nil"
  (when (looking-at "\\<[a-zA-Z0-9_]+\\>")
    (match-string 0)))

(defun comma-previous-indentation ()
  "Returns the indentation of the line preceding point."
  (save-excursion
    (if (comma-back-line) (current-indentation) 0)))

(defun comma-next-indentation ()
  "Returns the indentation of the line after point."
  (save-excursion
    (if (comma-forward-line) (current-indentation) 0)))

(defun comma-match-eol (regex)
  "Returns true if the given regex matches the end of the current
line (ignoring comments)."
  (re-search-forward (concat regex "[ \t]*\\(--.*$\\)?")
                     (line-end-position) t))


;;;===-----------------------------------------------------------------------===
;;; Indentation Engine.

(defun comma-indent-after-subroutine ()
  "Assumes point is looking at either a function or procedure.
Returns the indentation which should follow relative to the
current position."
  ;; Seek either a left paren, "is", semicolon, or newline.
  (let ((pos (point)))
    (if (re-search-forward "(\\|;\\|$\\|\\<is\\>" nil t)
        (let* ((match-pos (match-beginning 0))
               (char      (char-after match-pos)))
          (cond
           ((char-equal char ?\;) 0)
           ((char-equal char ?\n) comma-default-indent)
           ((char-equal char ?i)  comma-default-indent)
           ((char-equal char ?\()
            ;; When we find a left paren discover on which line its mate is
            ;; located.
            (let ((paren-indentation (1+ (- match-pos pos)))
                  (paren-line        (line-number-at-pos)))
              (condition-case nil
                  (let* ((close-pos  (scan-lists match-pos 1 0))
                         (close-line (line-number-at-pos close-pos)))
                    (cond
                     ((= paren-line close-line)
                      ;; The closing paren is on the same line. See if it is
                      ;; followed by a semicolon.
                      (goto-char close-pos)
                      (if (search-forward ";" (line-end-position) t)
                          0
                        comma-default-indent))

                     ;; Different lines.  Indent one past the left paren.
                     (t paren-indentation)))
                (error paren-indentation)))))))))

(defun comma-indent-return ()
  "Assumes point is looking at the `return' reserved word.  Returns
the amount the return should be indented."
  ;; A return reserved word either follows a function declaration or is a
  ;; statement.  If a ')' ends the previous line (ignoring comments) or the
  ;; reserved word `function' begins the line this return is part of a function
  ;; declaration.
  (let ((indentation
         (save-excursion
           (comma-back-line)
           (back-to-indentation)
           (cond
            ((looking-at "\\<function\\>")
             (+ (current-column) comma-return-indent))
            ((comma-match-eol ")")
             (if (re-search-backward "\\<function\\>" nil t)
                 (+ (current-column) comma-return-indent)
               ;; No function reserved word.  Probably a syntax error.
               (comma-previous-indentation)))))))
    (if indentation
        indentation
      (comma-indent-statement))))

(defun comma-indent-after-return ()
  "Assumes point is looking at the `return' reserved word.  Returns the amount
to indent the line after, relative to the current column."
  (cond
   ;; If an `is' ends the line the declarative region of a function body
   ;; follows.
   ((comma-match-eol "\\<is\\>")
    (- comma-default-indent comma-return-indent))
   ;; If a `;' ends the line, indent relative to the `return'.
   ((comma-match-eol ";") 0)
   ;; Otherwise, the return statement spans multiple lines.
   (t comma-default-indent)))

(defun comma-indent-after-is ()
  "Assumes an `is' reserved word terminates the current line.  Returns the
amount to indent the line after.  This is an absolute indentation."
  (let ((prefix (current-column)))
    (cond
     ((looking-at "\\<\\(type\\|subtype\\|domain\\)\\>")
      (+ prefix comma-default-indent))
     ((re-search-backward "\\<\\(function\\|procedure\\|domain\\)\\>" nil t)
      (+ (current-column) comma-default-indent))
     (t prefix))))

(defun comma-indent-statement ()
  "Assumes point is looking at a statement, and that there is no
  `interesting' context (like reserved words).  Returns the
  amount by which the statement should be indented."
  ;; FIXME: We should find the indentation of the previous statement or find a
  ;; reserved word like `being', `then', `loop', etc, to establish context.
  (save-excursion
    (cond
     ((not (comma-back-line)) 0)
     ((comma-match-eol ";") (current-indentation))
     (t (+ (current-indentation) comma-default-indent)))))

(defun comma-find-if-indentation ()
  "Returns the indentation of the previous `if' token."
  (when (re-search-backward "\\<if\\>" nil t)
    (current-column)))

(defun comma-find-iteration-indentation ()
  "Returns the indentation of the previous iteration construct (`loop', `while',
or `for')."
  (when (re-search-backward "\\<for\\|while\\|loop\\>" nil t)
    (let ((char (char-after)))
      (cond
       ((char-equal ?f char) (current-column))
       ((char-equal ?w char) (current-column))
       (t
        ;; We need to determine if this loop token is preceeded by a for or a
        ;; while.
        (let ((loop-pos (current-indentation)))
          (if (re-search-backward "\\<for\\|while\\|begin\\>\\|;" nil t)
              (let ((char (char-after)))
                (cond
                 ((char-equal ?f char) (current-column))
                 ((char-equal ?w char) (current-column))
                 ((char-equal ?b char) loop-pos)
                 (t loop-pos)))
            loop-pos)))))))

(defun comma-find-end-tag-indentation (end-tag)
  "Returns the indentation of the construct which introcduces the
given END-TAG or nil if no such construct could be found."
  ;; Simple search for now.  Look for domain, signature, function, and
  ;; procedure.
  ;;
  ;; FIXME: Handle labels.
  (let ((context-regex
         (eval-when-compile
           (concat "\\<"
                   (regexp-opt '("function" "procedure" "domain" "signature") t)
                   "\\>")))
        (tag-regex (concat "\\<" end-tag "\\>")))
    (catch 'finish
      (while (re-search-backward context-regex nil t)
        (let ((indentation (current-column)))
          (save-excursion
            (forward-word)
            (comma-forward-whitespace)
            (when (looking-at tag-regex)
              (throw 'finish indentation))))))))

(defun comma-find-end-indentation ()
  "Assuming we are looking at an `end' token, returns the amount
the token should be indented by."
  ;; Look at the token following `end' to determine the context.
  (forward-word)
  (comma-forward-whitespace)
  (cond
   ((looking-at "\\<if\\>") (comma-find-if-indentation))
   ((looking-at "\\<loop\\>") (comma-find-iteration-indentation))
   (t (let ((end-tag (comma-scan-identifier)))
        (comma-find-end-tag-indentation end-tag)))))

(defun comma-find-dedentation ()
  "Returns the position the current line should be indented to or
nil.  This function performs leftward indentation and alignment."
  (save-excursion
    (back-to-indentation)
    (let ((prefix (current-indentation)))
      (cond
       ((looking-at "\\<begin\\>")
        ;; Locate the preceeding declare, function, or procedure word.
        (when (re-search-backward "\\<declare\\|procedure\\|function\\>" nil t)
          (current-column)))
       ((looking-at "\\<else\\|elsif\\>")
        (comma-find-if-indentation))
       ((looking-at "\\<end\\>")
        (comma-find-end-indentation))
       ((looking-at "\\<add\\>")
        (when (re-search-backward "\\<domain\\>" nil t)
          (current-column)))
       (t nil)))))

(defun comma-find-indentation ()
  "Returns the position the current line should be indented to or
nil.  This function performs rightward indentation and alignment."
  (let ((comb-regex
         (eval-when-compile
           (concat "\\<"
                   (regexp-opt '("add" "domain" "signature" "while" "for" "loop"
                                 "begin" "if" "else" "elsif" "generic") t)
                   "\\>"))))
    (save-excursion
      (back-to-indentation)
      ;; First phase looks at the current line,
      (cond
       ((looking-at "\\<return\\>")
        (comma-indent-return))
       ;; Second phase inspects the previous line.
       (t (if (not (comma-back-line))
              0
            (back-to-indentation)
            (let ((prefix (current-indentation)))
              (cond
               ((looking-at "\\<procedure\\|function\\>")
                (+ prefix (comma-indent-after-subroutine)))
               ((looking-at "\\<return\\>")
                (+ prefix (comma-indent-after-return)))
               ((comma-match-eol "\\<is\\>")
                (comma-indent-after-is))
               ((looking-at comb-regex)
                (+ prefix comma-default-indent))
               (t nil)))))))))

(defun comma-resolve-indentation ()
  "Returns the prefered indentation level for the current line."
  (or (comma-find-dedentation)
      (comma-find-indentation)
      (comma-indent-statement)))

(defun comma-indent-current-line ()
  "Indents the current line."
  (interactive)
  (let ((indentation (comma-resolve-indentation)))
    (when indentation
      (save-excursion
        (beginning-of-line)
        (delete-horizontal-space)
        (indent-to indentation))
      (if (comma-at-start-of-line)
          (back-to-indentation)))))

;;;===-----------------------------------------------------------------------===
;;; Mode Definition.

(defun comma-mode ()
  "Major mode for editing Comma code."

  (interactive)
  (comma-create-syntax-table)
  (set-syntax-table comma-mode-syntax-table)

  (set 'comment-start "-- ")

  (set (make-local-variable 'font-lock-defaults)
       '(comma-font-lock-keywords))

  (set (make-local-variable 'indent-line-function)
       'comma-indent-current-line)

  (use-local-map (comma-create-keymap))

  (setq major-mode 'comma-mode
        mode-name   "Comma"))

;;;===-----------------------------------------------------------------------===
;;; Global Initialization.

(add-to-list 'auto-mode-alist '("\\.cms$" . comma-mode))

(provide 'comma-mode)
