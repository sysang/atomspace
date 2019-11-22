(use-modules (opencog) (opencog exec))

(List (Concept "A")
	(Set (Predicate "P") (Predicate "Q") (Predicate "R")))

; Expect 3!=6 solutions
(define odo-dim-one
	(Bind
		(Present (List (Variable "$C")
			(Set (Variable "$X") (Variable "$Y") (Variable "$Z"))))
		(Associative
			(Variable "$X") (Variable "$Y") (Variable "$Z"))))

; ----------------------------------------------------
; Like above, but 3! * 3! = 36 permutations

(List (Concept "B")
	(Set (Predicate "P") (Predicate "Q") (Predicate "R"))
	(Set (Predicate "S") (Predicate "T") (Predicate "U")))

(define odo-dim-two
	(Bind
		(Present (List (Variable "$C")
			(Set (Variable "$U") (Variable "$V") (Variable "$W"))
			(Set (Variable "$X") (Variable "$Y") (Variable "$Z"))))
		(Associative
			(Variable "$U") (Variable "$V") (Variable "$W")
			(Variable "$X") (Variable "$Y") (Variable "$Z"))))

; ----------------------------------------------------
; Like above, but 6*6*6 = 216 permutations
(List (Concept "C")
	(Set (Predicate "P") (Predicate "Q") (Predicate "R"))
	(Set (Predicate "S") (Predicate "T") (Predicate "U"))
	(Set (Predicate "V") (Predicate "W") (Predicate "X")))

(define odo-dim-three
	(Bind
		(Present (List (Variable "$C")
			(Set (Variable "$A") (Variable "$B") (Variable "$C"))
			(Set (Variable "$U") (Variable "$V") (Variable "$W"))
			(Set (Variable "$X") (Variable "$Y") (Variable "$Z"))))
		(Associative
			(Variable "$A") (Variable "$B") (Variable "$C")
			(Variable "$U") (Variable "$V") (Variable "$W")
			(Variable "$X") (Variable "$Y") (Variable "$Z"))))

; ----------------------------------------------------
; Like above, but 6*6*6*6 = 1296 permutations
(List (Concept "D")
	(Set (Predicate "L") (Predicate "M") (Predicate "N"))
	(Set (Predicate "P") (Predicate "Q") (Predicate "R"))
	(Set (Predicate "S") (Predicate "T") (Predicate "U"))
	(Set (Predicate "V") (Predicate "W") (Predicate "X")))

(define odo-dim-four
	(Bind
		(Present (List (Variable "$C")
			(Set (Variable "$A") (Variable "$B") (Variable "$C"))
			(Set (Variable "$D") (Variable "$E") (Variable "$F"))
			(Set (Variable "$U") (Variable "$V") (Variable "$W"))
			(Set (Variable "$X") (Variable "$Y") (Variable "$Z"))))
		(Associative
			(Variable "$A") (Variable "$B") (Variable "$C")
			(Variable "$D") (Variable "$E") (Variable "$F")
			(Variable "$U") (Variable "$V") (Variable "$W")
			(Variable "$X") (Variable "$Y") (Variable "$Z"))))
