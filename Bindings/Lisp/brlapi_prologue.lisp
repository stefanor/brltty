;;;; libbrlapi - A library providing access to braille terminals for applications.
;;;;
;;;; Copyright (C) 2006-2018 by Mario Lang <mlang@delysid.org>
;;;;
;;;; libbrlapi comes with ABSOLUTELY NO WARRANTY.
;;;;
;;;; This is free software, placed under the terms of the
;;;; GNU Lesser General Public License, as published by the Free Software
;;;; Foundation; either version 2.1 of the License, or (at your option) any
;;;; later version. Please see the file LICENSE-LGPL for details.
;;;;
;;;; Web Page: http://brltty.com/
;;;;
;;;; This software is maintained by Dave Mielke <dave@mielke.cc>.

(eval-when (:compile-toplevel)
  (declaim (optimize (safety 3) (debug 3))))

;;;; * Package definition

(defpackage :brlapi
  (:use :common-lisp :cffi)
  (:export #:open-connection #:close-connection #:is-connected
           #:print-properties #:print-object
           #:library-version
	   #:driver-name #:model-identifier #:display-size
           #:enter-tty-mode #:leave-tty-mode
           #:write-text #:write-dots #:write-region
           #:read-key #:expand-key-code
           #:error-message))

(in-package :brlapi)

